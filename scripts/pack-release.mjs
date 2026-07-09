#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { allPublishDirs, readJson, root } from './packages.mjs';

const destination = path.resolve(process.argv[2] || path.join(root, 'dist', 'npm'));

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: root,
    stdio: 'inherit',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function verifyPrebuilds(dir) {
  const prebuildsFile = path.join(root, dir, 'prebuilds.json');
  if (!fs.existsSync(prebuildsFile)) return;

  const manifest = readJson(prebuildsFile);
  const expected = manifest.binaries
    .map((binary) => path.basename(binary.path))
    .sort();
  const prebuiltDir = path.join(root, dir, 'prebuilt');
  const actual = fs.existsSync(prebuiltDir)
    ? fs.readdirSync(prebuiltDir).filter((name) => name.endsWith('.node')).sort()
    : [];
  const missing = expected.filter((name) => !actual.includes(name));
  const extra = actual.filter((name) => !expected.includes(name));
  if (missing.length || extra.length) {
    throw new Error([
      `prebuild mismatch for ${dir}`,
      missing.length ? `missing: ${missing.join(', ')}` : '',
      extra.length ? `extra: ${extra.join(', ')}` : '',
    ].filter(Boolean).join('\n'));
  }
}

function tarballName(manifest) {
  if (manifest.name.startsWith('@')) {
    return `${manifest.name.slice(1).replace('/', '-')}-${manifest.version}.tgz`;
  }
  return `${manifest.name}-${manifest.version}.tgz`;
}

fs.rmSync(destination, { recursive: true, force: true });
fs.mkdirSync(destination, { recursive: true });

const publishOrder = [];
for (const dir of allPublishDirs()) {
  const manifest = readJson(path.join(root, dir, 'package.json'));
  verifyPrebuilds(dir);
  run('pnpm', ['--dir', dir, 'pack', '--pack-destination', destination]);

  const tarball = tarballName(manifest);
  const tarballPath = path.join(destination, tarball);
  if (!fs.existsSync(tarballPath)) {
    throw new Error(`expected pack output not found: ${tarballPath}`);
  }
  publishOrder.push(tarball);
}

fs.writeFileSync(path.join(destination, 'publish-order.txt'), `${publishOrder.join('\n')}\n`);
console.log(`Packed ${publishOrder.length} packages into ${path.relative(root, destination)}`);
