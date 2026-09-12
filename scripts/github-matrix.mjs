#!/usr/bin/env node

import path from 'node:path';

import { FAMILIES, platformDirsFor, readJson, root } from './packages.mjs';
import { electronReleaseCatalog } from './electron-releases.mjs';

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

const ELECTRON_ARCH = {
  'darwin-arm64': 'arm64',
  'darwin-x64': 'x64',
  'linux-arm64-gnu': 'arm64',
  'linux-x64-gnu': 'x64',
  'win32-arm64-msvc': 'arm64',
  'win32-ia32-msvc': 'ia32',
  'win32-x64-msvc': 'x64',
};

const ELECTRON_PLATFORM = {
  'darwin-arm64': 'darwin',
  'darwin-x64': 'darwin',
  'linux-arm64-gnu': 'linux',
  'linux-x64-gnu': 'linux',
  'win32-arm64-msvc': 'win32',
  'win32-ia32-msvc': 'win32',
  'win32-x64-msvc': 'win32',
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

function electronPlatformMatrix() {
  return {
    include: ciPlatformMatrix().include
      .filter((entry) => ELECTRON_ARCH[entry.platform])
      .map((entry) => {
        const versions = electronReleaseCatalog.versionsForPlatform(entry.platform);
        if (versions.length === 0) {
          throw new Error(`missing Electron releases for ${entry.platform}`);
        }
        return {
          platform: entry.platform,
          runner: entry.runner,
          build_node: entry.build_node,
          electron_platform: ELECTRON_PLATFORM[entry.platform],
          electron_versions: versions.join(' '),
          arch: ELECTRON_ARCH[entry.platform],
          families: entry.families,
          artifact_pattern: `prebuild-*-${entry.platform}-napi-v9`,
          ...(entry.nodearch
            ? {
                // Electron 43's installer has no win32-ia32 extraction binary.
                // Download the official ia32 distribution from an x64 host
                // process, then switch back to x86 Node for the test driver.
                nodearch: 'x64',
                test_nodearch: entry.nodearch,
              }
            : {}),
        };
      }),
  };
}

// Distributions that package their own Node, tested against the prebuilds.
// A distribution build is not just a different version: it is a different binary,
// usually a thin /usr/bin/node against a shared libnode.so, compiled with
// hardening the nodejs.org binaries do not use. That hardening changes the
// machine-code shape of the private getter the runtime probe decodes, so it needs
// coverage against the real packaged binary.
//
// What CI has actually measured on the packaged aarch64 libnode, and why the
// table looks like this. paciasp counts are what distinguish the cases:
//
//   Fedora 44/rawhide - `%{build_cflags}` combine `-mbranch-protection=standard`
//               with `-mno-omit-leaf-frame-pointer`, so PAC reaches leaf functions
//               too (74839-119961 paciasp) and even this leaf accessor gets a frame
//               record: a 28-byte, 7-instruction body. Still the only shape that
//               needs the wide window.
//   Debian 13 - branch protection on, but as pac-ret without +leaf and without
//               forcing a leaf frame, so only non-leaf functions carry PAC (6241)
//               and the getter stays `bti c; ldr; ret`, 12 bytes.
//   Ubuntu 26.04 - same pattern as Debian (6682), Node 22.22.1.
//   RHEL 9 rebuilds - Rocky and AlmaLinux carry PAC in their nodejs:20/22 module
//               streams (6319), also pac-ret rather than +leaf. Their *default*
//               nodejs is 16, below this project's floor, which is why the stream
//               is selected explicitly.
//   Amazon Linux 2023 - heavy PAC and BTI (55814 / 30149) but again pac-ret, so a
//               12-byte getter. Its default `nodejs` is 18 and statically linked;
//               the versioned nodejs20/nodejs22 packages use the shared layout.
//   Arch      - CET only, no `-mbranch-protection`.
//   Ubuntu 24.04 - no branch protection at all, and Node 18. Not in the matrix.
//
// Two conclusions worth keeping, because both contradict a plausible guess:
// the distinguishing flag is `-mno-omit-leaf-frame-pointer` rather than branch
// protection on its own, and static linking is not itself disqualifying — Rocky's
// nodejs:20 and openSUSE's Node are statically linked yet still export the private
// symbol, so the probe resolves it. What breaks is static linking *plus* hidden
// private symbols, which is what Amazon Linux's default nodejs 18 does.
//
// Nothing here is optional: every entry has been confirmed by a run to package a
// Node this project supports, so a failure is a real regression and must fail the
// build.
//
// Fields are shell snippets evaluated inside the container. `$V` is the stream
// being tested, and `$node_bin` is the resolved interpreter path, available to
// `probe` because it runs after `binary` is expanded. Distributions that ship
// exactly one Node use the stream name `default` and ignore $V: version coverage
// comes from Fedora's parallel streams, while these entries exist to cover each
// distribution's build flags, which is what changes the getter shape. binutils
// rides along with the install because the hardening census uses objdump.
//
// The snippets run under `set -u`, so they must not contain `${...}` sequences
// meant for another program — a dpkg-query format placeholder would be read as an
// unset shell variable and abort the run.
const DISTROS = [
  {
    id: 'fedora-44',
    image: 'registry.fedoraproject.org/fedora:44',
    streams: [20, 22, 24],
    install: 'dnf install -y --setopt=install_weak_deps=False nodejs$V binutils',
    binary: '/usr/bin/node-$V',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'fedora-rawhide',
    image: 'registry.fedoraproject.org/fedora:rawhide',
    // Where Node 26 landed first (nodejs26-26.3.1-5.fc45). Rolling by
    // definition, so its churn warns rather than fails.
    streams: [22, 24, 26],
    install: 'dnf install -y --setopt=install_weak_deps=False nodejs$V binutils',
    binary: '/usr/bin/node-$V',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'debian-13',
    image: 'docker.io/library/debian:trixie',
    // trixie ships Node 20.19.2, confirmed via sources.debian.org, and its
    // getter bytes were read out of the packaged libnode115.
    streams: ['default'],
    install:
      'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends nodejs binutils',
    binary: '/usr/bin/node',
    // No ${...} in this: these snippets are evaluated by a shell running with
    // `set -u`, so a dpkg-query format placeholder would be read as an unset
    // shell variable and abort.
    probe: 'dpkg -S "$node_bin"',
  },
  {
    id: 'rocky-9',
    image: 'quay.io/rockylinux/rockylinux:9',
    // RHEL 9's default nodejs is 16, below this project's floor, so the module
    // stream is selected explicitly. Its packaged aarch64 libnode was measured
    // with paciasp=0 and effectively no `bti c`: RHEL 9 predates the aarch64
    // branch protection Fedora now enables, so this covers an unhardened
    // enterprise rebuild rather than the Fedora shape. Verifying that the Fedora
    // shape reaches enterprise builds needs a RHEL 10 generation image.
    streams: [20, 22],
    install: 'dnf module install -y nodejs:$V && dnf install -y binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'almalinux-9',
    image: 'quay.io/almalinuxorg/almalinux:9',
    streams: [20, 22],
    install: 'dnf module install -y nodejs:$V && dnf install -y binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'amazonlinux-2023',
    image: 'public.ecr.aws/amazonlinux/amazonlinux:2023',
    // Its default nodejs is 18 and, more importantly, statically linked with no
    // shared libnode, so the private getter symbol is not exported and the probe
    // cannot resolve it at all. The versioned packages are tried in case those
    // ship the shared layout; if they do not, this distribution simply cannot be
    // supported, which is why the entry stays optional.
    streams: [20, 22],
    install: 'dnf install -y nodejs$V binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'opensuse-leap-15',
    image: 'registry.opensuse.org/opensuse/leap:15.6',
    // Passing already; kept optional until a couple of runs confirm stability.
    streams: ['default'],
    install: 'zypper --non-interactive --gpg-auto-import-keys install nodejs binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -qf "$node_bin"',
  },
  {
    id: 'ubuntu-2604',
    image: 'public.ecr.aws/ubuntu/ubuntu:26.04',
    // 24.04 was measured and deliberately skipped: no branch protection and its
    // Node is 18. This entry only pays off if 26.04 ships Node 20 or newer,
    // hence optional.
    streams: ['default'],
    install:
      'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends nodejs binutils',
    binary: '/usr/bin/node',
    probe: 'dpkg -S "$node_bin"',
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
        distro_install: distro.install,
        distro_binary: distro.binary,
        distro_probe: distro.probe,
      });
    }
  }
  return { include };
}

const target = process.argv[2];
const matrices = {
  'ci-platforms': ciPlatformMatrix,
  'ci-electron-platforms': electronPlatformMatrix,
  'ci-hmr-platforms': hmrPlatformMatrix,
  'ci-distro-platforms': distroPlatformMatrix,
  'release-platforms': platformMatrix,
};

if (!target || !matrices[target]) {
  console.error(`Usage: node scripts/github-matrix.mjs <${Object.keys(matrices).join('|')}>`);
  process.exit(1);
}

process.stdout.write(JSON.stringify(matrices[target]()));
