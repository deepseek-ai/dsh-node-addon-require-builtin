#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const packagesRoot = path.join(root, 'packages');
const artifactRoot = path.resolve(process.argv[2] || '.release/prebuild-artifacts');

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function platformDirs() {
  return fs.readdirSync(packagesRoot)
    .filter((name) => name !== 'entry' && name !== 'loader')
    .filter((name) => fs.existsSync(path.join(packagesRoot, name, 'prebuilds.json')))
    .sort();
}

function expectedFiles(platform) {
  const manifest = readJson(path.join(packagesRoot, platform, 'prebuilds.json'));
  return manifest.binaries
    .map((binary) => path.basename(binary.path))
    .sort();
}

function actualFiles(platform) {
  const prebuiltDir = path.join(packagesRoot, platform, 'prebuilt');
  if (!fs.existsSync(prebuiltDir)) return [];
  return fs.readdirSync(prebuiltDir)
    .filter((name) => name.endsWith('.node'))
    .sort();
}

function copyArtifacts(platforms) {
  if (!fs.existsSync(artifactRoot)) {
    throw new Error(`prebuild artifact directory does not exist: ${artifactRoot}`);
  }

  for (const platform of platforms) {
    const prebuiltDir = path.join(packagesRoot, platform, 'prebuilt');
    fs.rmSync(prebuiltDir, { recursive: true, force: true });
    fs.mkdirSync(prebuiltDir, { recursive: true });
  }

  const knownPlatforms = new Set(platforms);
  for (const artifactName of fs.readdirSync(artifactRoot)) {
    const artifactDir = path.join(artifactRoot, artifactName);
    if (!fs.statSync(artifactDir).isDirectory()) continue;

    const platform = platforms.find((candidate) => artifactName.startsWith(`prebuild-${candidate}-`));
    if (!platform || !knownPlatforms.has(platform)) {
      throw new Error(`cannot infer platform from artifact: ${artifactName}`);
    }

    const nodes = fs.readdirSync(artifactDir).filter((name) => name.endsWith('.node'));
    if (nodes.length !== 1) {
      throw new Error(`expected exactly one .node file in ${artifactDir}, found ${nodes.length}`);
    }

    const source = path.join(artifactDir, nodes[0]);
    const destination = path.join(packagesRoot, platform, 'prebuilt', nodes[0]);
    fs.copyFileSync(source, destination);
    console.log(`Copied ${path.relative(root, source)} -> ${path.relative(root, destination)}`);
  }
}

function verify(platforms) {
  for (const platform of platforms) {
    const expected = expectedFiles(platform);
    const actual = actualFiles(platform);
    const missing = expected.filter((name) => !actual.includes(name));
    const extra = actual.filter((name) => !expected.includes(name));
    if (missing.length || extra.length) {
      throw new Error([
        `prebuild mismatch for ${platform}`,
        missing.length ? `missing: ${missing.join(', ')}` : '',
        extra.length ? `extra: ${extra.join(', ')}` : '',
      ].filter(Boolean).join('\n'));
    }
    console.log(`Verified ${platform}: ${actual.length} prebuilds`);
  }
}

const platforms = platformDirs();
copyArtifacts(platforms);
verify(platforms);
