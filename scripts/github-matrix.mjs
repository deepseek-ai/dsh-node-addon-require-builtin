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
// What the measurements so far show, and why the table looks like this:
//
//   Fedora 44 - `%{build_cflags}` on aarch64 combine `-mbranch-protection=standard`
//               with `-mno-omit-leaf-frame-pointer`. Together those give even this
//               leaf accessor pointer authentication and a frame record: a 28-byte,
//               7-instruction body, the only shape known to need the wide window.
//   Debian 13 - also enables branch protection (6241 paciasp in trixie's aarch64
//               libnode) but with pac-ret rather than +leaf, and without forcing a
//               leaf frame. The getter comes out `bti c; ldr; ret`, 12 bytes.
//   RHEL 9    - paciasp=0 and effectively no `bti c` in Rocky/Alma 9's aarch64
//               libnode: the enterprise generation predates the aarch64 branch
//               protection Fedora now applies. Its default Node is also 16, so the
//               module stream has to be selected explicitly.
//   Ubuntu    - 24.04 has no branch protection at all (0 paciasp, 0 bti c) and its
//               Node is 18, below this project's floor.
//   Arch      - enables CET but not `-mbranch-protection`.
//   AL2023    - links Node statically with no shared libnode, so the private
//               getter symbol is not exported and the probe cannot resolve it at
//               all. That is a hard limit of the packaging, not a decoder gap.
//
// So the distinguishing flag is `-mno-omit-leaf-frame-pointer`, not branch
// protection on its own, and it is not simply "RHEL family inherits Fedora" —
// RHEL 9 predates it. Fedora is currently the only shape needing the wide window.
//
// Each entry pins a currently-maintained release rather than the newest or the
// oldest. Entries are optional until a run confirms the distribution actually
// packages a Node this project supports, so an unverified guess surfaces as a
// warning instead of failing a PR.
//
// Fields are shell snippets evaluated inside the container, where $V is the
// stream being tested. Distributions that ship exactly one Node use the stream
// name `default` and ignore $V: version coverage comes from Fedora's parallel
// streams, while these entries exist to cover each distribution's build flags,
// which is what changes the getter shape. binutils rides along with the install
// because the hardening census uses objdump; it is optional at runtime.
//
// The snippets run under `set -u`, so they must not contain `${...}` sequences
// meant for another program — a dpkg-query format placeholder would be read as an
// unset shell variable and abort the run.
const DISTROS = [
  {
    id: 'fedora-44',
    image: 'registry.fedoraproject.org/fedora:44',
    optional: false,
    streams: [20, 22, 24],
    install: 'dnf install -y --setopt=install_weak_deps=False nodejs$V binutils',
    binary: '/usr/bin/node-$V',
    probe: 'rpm -q nodejs$V-libs',
  },
  {
    id: 'fedora-rawhide',
    image: 'registry.fedoraproject.org/fedora:rawhide',
    // Where Node 26 landed first (nodejs26-26.3.1-5.fc45). Rolling by
    // definition, so its churn warns rather than fails.
    optional: true,
    streams: [22, 24, 26],
    install: 'dnf install -y --setopt=install_weak_deps=False nodejs$V binutils',
    binary: '/usr/bin/node-$V',
    probe: 'rpm -q nodejs$V-libs',
  },
  {
    id: 'debian-13',
    image: 'docker.io/library/debian:trixie',
    // trixie ships Node 20.19.2, confirmed via sources.debian.org, and its
    // getter bytes were read out of the packaged libnode115.
    optional: false,
    streams: ['default'],
    install:
      'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends nodejs binutils',
    binary: '/usr/bin/node',
    // No ${...} in this: these snippets are evaluated by a shell running with
    // `set -u`, so a dpkg-query format placeholder would be read as an unset
    // shell variable and abort.
    probe: 'dpkg-query -W nodejs',
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
    optional: true,
    streams: [20, 22],
    install: 'dnf module install -y nodejs:$V && dnf install -y binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -q nodejs',
  },
  {
    id: 'almalinux-9',
    image: 'quay.io/almalinuxorg/almalinux:9',
    optional: true,
    streams: [20, 22],
    install: 'dnf module install -y nodejs:$V && dnf install -y binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -q nodejs',
  },
  {
    id: 'amazonlinux-2023',
    image: 'public.ecr.aws/amazonlinux/amazonlinux:2023',
    // Its default nodejs is 18 and, more importantly, statically linked with no
    // shared libnode, so the private getter symbol is not exported and the probe
    // cannot resolve it at all. The versioned packages are tried in case those
    // ship the shared layout; if they do not, this distribution simply cannot be
    // supported, which is why the entry stays optional.
    optional: true,
    streams: [20, 22],
    install: 'dnf install -y nodejs$V binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -q nodejs$V',
  },
  {
    id: 'opensuse-leap-15',
    image: 'registry.opensuse.org/opensuse/leap:15.6',
    // Passing already; kept optional until a couple of runs confirm stability.
    optional: true,
    streams: ['default'],
    install: 'zypper --non-interactive --gpg-auto-import-keys install nodejs binutils',
    binary: '/usr/bin/node',
    probe: 'rpm -q nodejs',
  },
  {
    id: 'ubuntu-2604',
    image: 'public.ecr.aws/ubuntu/ubuntu:26.04',
    // 24.04 was measured and deliberately skipped: no branch protection and its
    // Node is 18. This entry only pays off if 26.04 ships Node 20 or newer,
    // hence optional.
    optional: true,
    streams: ['default'],
    install:
      'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends nodejs binutils',
    binary: '/usr/bin/node',
    probe: 'dpkg-query -W nodejs',
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
