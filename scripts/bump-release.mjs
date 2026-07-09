#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { allPublishDirs, readJson, root, versionedFiles } from './packages.mjs';

const bump = process.argv[2];
const releaseTypes = new Set(['major', 'minor', 'patch']);

function writeJson(file, value) {
  fs.writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`);
}

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: root,
    stdio: 'inherit',
    env: {
      ...process.env,
      CI: 'true',
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function parseVersion(version) {
  const match = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.exec(version);
  if (!match) {
    throw new Error(`invalid semver version: ${version}`);
  }
  return match.slice(1).map((part) => Number(part));
}

function nextVersion(current, release) {
  if (/^\d+\.\d+\.\d+$/.test(release)) return release;

  if (!releaseTypes.has(release)) {
    throw new Error('Usage: pnpm release:bump <major|minor|patch|x.y.z>');
  }

  const [major, minor, patch] = parseVersion(current);
  if (release === 'major') return `${major + 1}.0.0`;
  if (release === 'minor') return `${major}.${minor + 1}.0`;
  return `${major}.${minor}.${patch + 1}`;
}

// The published packages share one version. Read the consensus from the
// publishable set only (the root/hmr manifests track along but do not define it).
function currentPublishedVersion() {
  const versions = new Set(
    allPublishDirs().map((dir) => readJson(path.join(root, dir, 'package.json')).version),
  );
  if (versions.size !== 1) {
    throw new Error(`published package versions differ: ${[...versions].join(', ')}`);
  }
  return [...versions][0];
}

if (!bump) {
  console.error('Usage: pnpm release:bump <major|minor|patch|x.y.z>');
  process.exit(1);
}

const files = versionedFiles();
const targetVersion = nextVersion(currentPublishedVersion(), bump);

for (const file of files) {
  const fullPath = path.join(root, file);
  const json = readJson(fullPath);
  json.version = targetVersion;
  writeJson(fullPath, json);
  console.log(`${file}: ${targetVersion}`);
}

run('pnpm', ['install', '--ignore-scripts', '--lockfile-only']);
run('node', ['./scripts/verify-release.mjs']);

console.log(`Release version bumped to ${targetVersion}`);
