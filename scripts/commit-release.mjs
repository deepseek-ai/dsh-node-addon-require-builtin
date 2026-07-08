#!/usr/bin/env node

import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
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

function readJsonFromNode(file, expression) {
  const script = `const value = require('./${file}'); console.log(${expression});`;
  const result = spawnSync(process.execPath, ['-e', script], {
    cwd: root,
    encoding: 'utf8',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.stderr.write(result.stderr);
    process.exit(result.status ?? 1);
  }
  return result.stdout.trim();
}

if (!bump) {
  console.error('Usage: pnpm release:commit <major|minor|patch|x.y.z>');
  process.exit(1);
}

run('node', ['./scripts/bump-release.mjs', bump]);

const version = readJsonFromNode('packages/entry/package.json', 'value.version');
run('git', [
  'add',
  'package.json',
  'hmr-comparison/package.json',
  'packages/*/package.json',
  'pnpm-lock.yaml',
]);
run('git', ['commit', '-m', `release: ${version}`]);

console.log(`Committed release ${version}. Create the tag manually: git tag v${version}`);
