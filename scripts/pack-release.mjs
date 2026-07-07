#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const packagesRoot = path.join(root, 'packages');
const destination = path.resolve(process.argv[2] || path.join(root, 'dist', 'npm'));

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function packageDirs() {
  const platformDirs = fs.readdirSync(packagesRoot)
    .filter((name) => name !== 'entry' && name !== 'loader')
    .filter((name) => fs.existsSync(path.join(packagesRoot, name, 'package.json')))
    .sort()
    .map((name) => path.join('packages', name));

  return [
    'packages/loader',
    ...platformDirs,
    'packages/entry',
  ];
}

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

function tarballName(manifest) {
  if (manifest.name.startsWith('@')) {
    return `${manifest.name.slice(1).replace('/', '-')}-${manifest.version}.tgz`;
  }
  return `${manifest.name}-${manifest.version}.tgz`;
}

fs.rmSync(destination, { recursive: true, force: true });
fs.mkdirSync(destination, { recursive: true });

const publishOrder = [];
for (const dir of packageDirs()) {
  const manifest = readJson(path.join(root, dir, 'package.json'));
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
