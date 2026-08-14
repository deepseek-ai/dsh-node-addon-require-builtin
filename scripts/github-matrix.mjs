#!/usr/bin/env node

import path from 'node:path';

import { FAMILIES, platformDirsFor, readJson, root } from './packages.mjs';

const RUNNERS = {
  'darwin-arm64': 'macos-15',
  'darwin-x64': 'macos-15-intel',
  'linux-arm64-gnu': 'ubuntu-24.04-arm',
  'linux-x64-gnu': 'ubuntu-24.04',
  'win32-arm64-msvc': 'windows-11-arm',
  // Keep release builds on the oldest supported hosted Windows image so a
  // runner/toolset update cannot silently raise the prebuild's OS baseline.
  'win32-ia32-msvc': 'windows-2022',
  'win32-x64-msvc': 'windows-2022',
};

const NODE_ARCH = {
  'win32-ia32-msvc': 'x86',
};

const BUILD_NODE = {
  'win32-ia32-msvc': 22,
};

function napiBinary(dir) {
  const manifest = readJson(path.join(root, dir, 'prebuilds.json'));
  const binary = manifest.binaries.find((item) => item.backend === 'napi');
  if (!binary) throw new Error(`missing napi binary declaration for ${dir}`);
  return binary;
}

function platformEntry(family, dir) {
  const platform = path.basename(dir);
  const entry = {
    family,
    platform,
    runner: RUNNERS[platform],
    build_node: BUILD_NODE[platform] || 24,
    filename: path.basename(napiBinary(dir).path),
    // Artifact names carry the family so both variants' per-platform builds
    // stay distinct through upload/download.
    artifact: `prebuild-${family}-${platform}-napi-v9`,
    prebuilt_path: `${dir}/prebuilt`,
  };
  if (!entry.runner) {
    throw new Error(`missing GitHub runner for platform: ${platform}`);
  }
  if (NODE_ARCH[platform]) {
    entry.nodearch = NODE_ARCH[platform];
  }
  return entry;
}

function platformMatrix() {
  const include = [];
  for (const family of FAMILIES) {
    for (const dir of platformDirsFor(family)) {
      include.push(platformEntry(family, dir));
    }
  }
  return { include };
}

// CI builds and tests every family on one runner per platform, rather than one
// runner per (family, platform). The per-job setup — checkout, toolchain, pnpm
// install, TypeScript build — is identical across families and is the bulk of a
// job's fixed cost, so paying it once per platform instead of once per family
// halves it. The prebuilt binaries themselves are per family and still built,
// verified and uploaded separately.
//
// Release deliberately keeps the unaggregated matrix: publishing wants each
// platform package's artifact produced by its own job.
function ciPlatformMatrix() {
  const platforms = new Map();
  for (const family of FAMILIES) {
    for (const dir of platformDirsFor(family)) {
      const platform = path.basename(dir);
      const entry = platformEntry(family, dir);
      if (!platforms.has(platform)) {
        platforms.set(platform, {
          platform,
          runner: entry.runner,
          build_node: entry.build_node,
          // Every family emits the same binary name for a given platform, since
          // it encodes only platform and ABI.
          filename: entry.filename,
          families: FAMILIES.join(' '),
          // Artifact names must stay per family because downstream jobs (hmr,
          // distro-node) resolve them by name. upload-artifact takes one name per
          // step, so ci.yml has one upload step per entry here; adding a family
          // means adding a step.
          uploads: [],
          ...(entry.nodearch ? { nodearch: entry.nodearch } : {}),
        });
      }
      platforms.get(platform).uploads.push({
        family,
        artifact: entry.artifact,
        path: `${entry.prebuilt_path}/${entry.filename}`,
      });
    }
  }
  return { include: [...platforms.values()] };
}

function hmrPlatformMatrix() {
  // The HMR harness exercises ESM loader cache invalidation, so it intentionally
  // runs against the whitelisted internal-loader product only.
  const family = 'internal-loader';
  return {
    include: platformDirsFor(family).map((dir) => platformEntry(family, dir)),
  };
}

// Distributions that package their own Node, tested against the prebuilds.
// A distribution build is not just a different version: it is a different binary,
// usually a thin /usr/bin/node against a shared libnode.so, compiled with
// hardening the nodejs.org binaries do not use. That hardening changes the
// machine-code shape of the private getter the runtime probe decodes, so it needs
// coverage against the real packaged binary.
//
// Measured, not assumed:
//   Fedora       - `rpm --eval %{build_cflags}` on aarch64 yields
//                  `-mbranch-protection=standard -mno-omit-leaf-frame-pointer`
//                  (plus stack protector/clash flags). That combination gives even
//                  this leaf accessor pointer authentication and a frame record: a
//                  7-instruction, 28-byte body. It is the shape that motivated the
//                  two-stage decode, and the only distribution shape known to need
//                  the wider window.
//   Ubuntu 24.04 - no branch protection at all (0 paciasp, 0 `bti c` across 76569
//                  functions in the packaged aarch64 libnode), and its Node is 18,
//                  below this project's floor. Not in the matrix.
//
// Each job prints the branch-protection census of the libnode it installed, so
// this table stays evidence-based: adding a distribution here is how we learn what
// its build flags do, rather than assuming.
//
// Node stream coverage needs two Fedora releases, since neither carries all four:
//   Fedora 44 -> nodejs20, nodejs22, nodejs24
//   rawhide   -> nodejs22, nodejs24, nodejs26 (26.3.1-5.fc45)
// Using both also gets the GCC version difference between them for free.
//
// Every entry is currently Fedora, so ci.yml issues dnf commands directly. Adding
// a distribution with a different package manager means moving those commands
// into this table. Measured elsewhere and deliberately absent: Ubuntu 24.04
// applies no branch protection at all and its Node is 18, below this project's
// floor; Arch enables CET but not `-mbranch-protection`.
const DISTROS = [
  {
    id: 'fedora-44',
    image: 'registry.fedoraproject.org/fedora:44',
    streams: [20, 22, 24],
    optional: false,
  },
  {
    id: 'fedora-rawhide',
    image: 'registry.fedoraproject.org/fedora:rawhide',
    // rawhide is where Node 26 landed first. It is a moving target by definition,
    // so it is optional: rawhide churn should surface as a warning rather than
    // fail a PR on something unrelated to this project.
    streams: [22, 24, 26],
    optional: true,
  },
];

// One job per (platform, distribution). Both product families and every Node
// stream run inside that one container: the streams are parallel installable and
// the families differ only in which prebuilt binary is loaded, so a single image
// pull and dnf transaction covers all of them. Fanning out per family and per
// stream would pay a job's worth of setup for each combination instead.
function distroPlatformMatrix() {
  // Platforms are the same set across families, so collect them once. The family
  // list travels with each job and is iterated in shell.
  const platforms = new Map();
  for (const family of FAMILIES) {
    for (const dir of platformDirsFor(family)) {
      const platform = path.basename(dir);
      // Only the glibc Linux platforms: these jobs run a distribution's own Node
      // package, which exists on Linux only.
      if (!platform.startsWith('linux-')) continue;
      if (platforms.has(platform)) continue;
      const entry = platformEntry(family, dir);
      platforms.set(platform, {
        platform,
        runner: entry.runner,
        build_node: entry.build_node,
      });
    }
  }

  const include = [];
  for (const base of platforms.values()) {
    for (const distro of DISTROS) {
      include.push({
        ...base,
        // Space separated so the job can iterate these in shell.
        families: FAMILIES.join(' '),
        // Downloads both families' prebuilds in one step; the job then moves each
        // into packages/<family>/<platform>/prebuilt.
        artifact_pattern: `prebuild-*-${base.platform}-napi-v9`,
        distro: distro.id,
        distro_image: distro.image,
        distro_streams: distro.streams.join(' '),
        distro_optional: distro.optional,
      });
    }
  }
  return { include };
}

const target = process.argv[2];
const matrices = {
  'ci-platforms': ciPlatformMatrix,
  'ci-hmr-platforms': hmrPlatformMatrix,
  'ci-distro-platforms': distroPlatformMatrix,
  'release-platforms': platformMatrix,
};

if (!target || !matrices[target]) {
  console.error(`Usage: node scripts/github-matrix.mjs <${Object.keys(matrices).join('|')}>`);
  process.exit(1);
}

process.stdout.write(JSON.stringify(matrices[target]()));
