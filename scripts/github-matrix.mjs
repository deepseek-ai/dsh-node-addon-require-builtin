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
// One job per (family, platform, distribution) walks every stream inside a single
// container rather than fanning out a job per stream: the streams are parallel
// installable, so this is one image pull and one dnf transaction instead of a
// job's worth of setup each.
//
// Every entry is currently Fedora, so ci.yml issues dnf commands directly. Adding
// a distribution with a different package manager means moving those commands
// into this table.
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

function distroPlatformMatrix() {
  // Only the glibc Linux platforms: these jobs run a distribution's own Node
  // package, which exists on Linux only.
  const include = [];
  for (const family of FAMILIES) {
    for (const dir of platformDirsFor(family)) {
      const platform = path.basename(dir);
      if (!platform.startsWith('linux-')) continue;
      const entry = platformEntry(family, dir);
      for (const distro of DISTROS) {
        include.push({
          ...entry,
          distro: distro.id,
          distro_image: distro.image,
          // Space separated so the job can iterate it in shell.
          distro_streams: distro.streams.join(' '),
          distro_optional: distro.optional,
        });
      }
    }
  }
  return { include };
}

const target = process.argv[2];
const matrices = {
  'ci-platforms': platformMatrix,
  'ci-hmr-platforms': hmrPlatformMatrix,
  'ci-distro-platforms': distroPlatformMatrix,
  'release-platforms': platformMatrix,
};

if (!target || !matrices[target]) {
  console.error(`Usage: node scripts/github-matrix.mjs <${Object.keys(matrices).join('|')}>`);
  process.exit(1);
}

process.stdout.write(JSON.stringify(matrices[target]()));
