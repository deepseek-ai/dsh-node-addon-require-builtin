#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';

import { allPlatformPackages, readJson, root } from './packages.mjs';

const artifactRoot = path.resolve(process.argv[2] || '.release/prebuild-artifacts');

function expectedFiles(dir) {
  const manifest = readJson(path.join(root, dir, 'prebuilds.json'));
  return manifest.binaries
    .map((binary) => path.basename(binary.path))
    .sort();
}

function actualFiles(dir) {
  const prebuiltDir = path.join(root, dir, 'prebuilt');
  if (!fs.existsSync(prebuiltDir)) return [];
  return fs.readdirSync(prebuiltDir)
    .filter((name) => name.endsWith('.node'))
    .sort();
}

function copyArtifacts(packages) {
  if (!fs.existsSync(artifactRoot)) {
    throw new Error(`prebuild artifact directory does not exist: ${artifactRoot}`);
  }

  for (const { dir } of packages) {
    const prebuiltDir = path.join(root, dir, 'prebuilt');
    fs.rmSync(prebuiltDir, { recursive: true, force: true });
    fs.mkdirSync(prebuiltDir, { recursive: true });
  }

  for (const artifactName of fs.readdirSync(artifactRoot)) {
    const artifactDir = path.join(artifactRoot, artifactName);
    if (!fs.statSync(artifactDir).isDirectory()) continue;

    // Artifacts are named prebuild-<family>-<platform>-napi-v9. Match the
    // longest (family, platform) pair so a family/platform whose name is a
    // prefix of another can never be misrouted.
    const target = packages.find(
      ({ family, platform }) => artifactName.startsWith(`prebuild-${family}-${platform}-`),
    );
    if (!target) {
      throw new Error(`cannot infer target package from artifact: ${artifactName}`);
    }

    const nodes = fs.readdirSync(artifactDir).filter((name) => name.endsWith('.node'));
    if (nodes.length !== 1) {
      throw new Error(`expected exactly one .node file in ${artifactDir}, found ${nodes.length}`);
    }

    const source = path.join(artifactDir, nodes[0]);
    const destination = path.join(root, target.dir, 'prebuilt', nodes[0]);
    fs.copyFileSync(source, destination);
    console.log(`Copied ${path.relative(root, source)} -> ${path.relative(root, destination)}`);
  }
}

function verify(packages) {
  for (const { dir } of packages) {
    const expected = expectedFiles(dir);
    const actual = actualFiles(dir);
    const missing = expected.filter((name) => !actual.includes(name));
    const extra = actual.filter((name) => !expected.includes(name));
    if (missing.length || extra.length) {
      throw new Error([
        `prebuild mismatch for ${dir}`,
        missing.length ? `missing: ${missing.join(', ')}` : '',
        extra.length ? `extra: ${extra.join(', ')}` : '',
      ].filter(Boolean).join('\n'));
    }
    console.log(`Verified ${dir}: ${actual.length} prebuilds`);
  }
}

const packages = allPlatformPackages();
copyArtifacts(packages);
verify(packages);
