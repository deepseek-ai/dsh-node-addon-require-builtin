#!/usr/bin/env node

import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { entryDirFor, FAMILIES, readJson, root } from './packages.mjs';

const bump = process.argv[2];

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

if (!bump) {
  console.error('Usage: pnpm release:commit <major|minor|patch|x.y.z>');
  process.exit(1);
}

run('node', ['./scripts/bump-release.mjs', bump]);

// All published packages share the bumped version; read it from the first
// family's entry manifest.
const version = readJson(
  path.join(root, entryDirFor(FAMILIES[0]), 'package.json'),
).version;
run('git', [
  'add',
  'package.json',
  'hmr-comparison/package.json',
  'packages/**/package.json',
  'pnpm-lock.yaml',
]);
run('git', ['commit', '-m', `release: ${version}`]);

console.log(`Committed release ${version}. Create the tag manually: git tag v${version}`);
