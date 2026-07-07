#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const packagesRoot = path.join(root, 'packages');

const RUNNERS = {
  'darwin-arm64': 'macos-15',
  'darwin-x64': 'macos-15-intel',
  'linux-arm64-gnu': 'ubuntu-24.04-arm',
  'linux-x64-gnu': 'ubuntu-24.04',
  'win32-arm64-msvc': 'windows-11-arm',
  'win32-ia32-msvc': 'windows-2025',
  'win32-x64-msvc': 'windows-2025',
};

const NODE_ARCH = {
  'win32-ia32-msvc': 'x86',
};

const CI_NODE_MAJORS = [22, 24, 26];

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function platformDirs() {
  return fs.readdirSync(packagesRoot)
    .filter((name) => name !== 'entry' && name !== 'loader')
    .filter((name) => fs.existsSync(path.join(packagesRoot, name, 'prebuilds.json')))
    .sort();
}

function platformManifest(platform) {
  return readJson(path.join(packagesRoot, platform, 'prebuilds.json'));
}

function nodeMajor(binary) {
  if (typeof binary.node !== 'string' || !/^\d+$/.test(binary.node)) {
    return null;
  }
  return Number(binary.node);
}

function nodeabiBinaries(platform) {
  return platformManifest(platform).binaries
    .filter((binary) => binary.backend === 'nodeabi')
    .map((binary) => ({ ...binary, nodeMajor: nodeMajor(binary) }))
    .filter((binary) => binary.nodeMajor !== null)
    .sort((a, b) => a.nodeMajor - b.nodeMajor);
}

function napiBuildNode(platform) {
  const supported = nodeabiBinaries(platform).map((binary) => binary.nodeMajor);
  if (supported.includes(24)) return 24;
  return Math.max(...supported);
}

function baseMatrixEntry(platform, node) {
  const entry = {
    platform,
    node,
    runner: RUNNERS[platform],
  };
  if (!entry.runner) {
    throw new Error(`missing GitHub runner for platform: ${platform}`);
  }
  if (NODE_ARCH[platform]) {
    entry.nodearch = NODE_ARCH[platform];
  }
  return entry;
}

function supportedMatrix() {
  const include = [];
  for (const platform of platformDirs()) {
    const supportedNodes = new Set(nodeabiBinaries(platform).map((binary) => binary.nodeMajor));
    for (const node of CI_NODE_MAJORS) {
      if (!supportedNodes.has(node)) continue;
      include.push({
        ...baseMatrixEntry(platform, node),
        optional: true,
      });
    }
  }
  return { include };
}

function releasePrebuildMatrix() {
  const include = [];
  for (const platform of platformDirs()) {
    const napiBinary = platformManifest(platform).binaries.find((binary) => binary.backend === 'napi');
    if (!napiBinary) {
      throw new Error(`missing napi binary declaration for ${platform}`);
    }
    include.push({
      ...baseMatrixEntry(platform, napiBuildNode(platform)),
      backend: 'napi',
      abi: napiBinary.abi,
      binaryTag: napiBinary.binaryTag,
      filename: path.basename(napiBinary.path),
      artifact: `prebuild-${platform}-${napiBinary.binaryTag}`,
    });

    for (const binary of nodeabiBinaries(platform)) {
      include.push({
        ...baseMatrixEntry(platform, binary.nodeMajor),
        backend: 'nodeabi',
        abi: binary.abi,
        binaryTag: binary.binaryTag,
        filename: path.basename(binary.path),
        artifact: `prebuild-${platform}-${binary.binaryTag}`,
      });
    }
  }
  return { include };
}

const target = process.argv[2];
const matrices = {
  'ci-supported': supportedMatrix,
  'ci-hmr': supportedMatrix,
  'release-prebuild': releasePrebuildMatrix,
};

if (!target || !matrices[target]) {
  console.error(`Usage: node scripts/github-matrix.mjs <${Object.keys(matrices).join('|')}>`);
  process.exit(1);
}

process.stdout.write(JSON.stringify(matrices[target]()));
