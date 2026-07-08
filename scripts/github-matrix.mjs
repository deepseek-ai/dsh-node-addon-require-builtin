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

const BUILD_NODE = {
  'win32-ia32-msvc': 22,
};

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

function napiBinary(platform) {
  const binary = platformManifest(platform).binaries.find((item) => item.backend === 'napi');
  if (!binary) throw new Error(`missing napi binary declaration for ${platform}`);
  return binary;
}

function platformEntry(platform) {
  const entry = {
    platform,
    runner: RUNNERS[platform],
    build_node: BUILD_NODE[platform] || 24,
    filename: path.basename(napiBinary(platform).path),
    artifact: `prebuild-${platform}-napi-v9`,
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
  return { include: platformDirs().map((platform) => platformEntry(platform)) };
}

const target = process.argv[2];
const matrices = {
  'ci-platforms': platformMatrix,
  'release-platforms': platformMatrix,
};

if (!target || !matrices[target]) {
  console.error(`Usage: node scripts/github-matrix.mjs <${Object.keys(matrices).join('|')}>`);
  process.exit(1);
}

process.stdout.write(JSON.stringify(matrices[target]()));
