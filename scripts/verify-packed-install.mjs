#!/usr/bin/env node

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const packagesRoot = path.join(root, 'packages');
const tarballDir = path.resolve(process.argv[2] || path.join(root, 'dist', 'npm'));
const entryPackageName = '@esplus/node-addon-require-builtin';
const loaderPackageName = '@esplus/node-addon-require-builtin-loader';

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

function packageManifests() {
  return packageDirs().map((dir) => ({
    dir,
    manifest: readJson(path.join(root, dir, 'package.json')),
  }));
}

function tarballName(manifest) {
  if (manifest.name.startsWith('@')) {
    return `${manifest.name.slice(1).replace('/', '-')}-${manifest.version}.tgz`;
  }
  return `${manifest.name}-${manifest.version}.tgz`;
}

function tarballPath(manifest) {
  const tarball = path.join(tarballDir, tarballName(manifest));
  if (!fs.existsSync(tarball)) {
    throw new Error(`missing packed tarball: ${tarball}`);
  }
  return tarball;
}

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: options.cwd || root,
    stdio: 'inherit',
    env: {
      ...process.env,
      ...options.env,
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function runCapture(command, args) {
  const result = spawnSync(command, args, {
    cwd: root,
    encoding: 'utf8',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.stderr.write(result.stderr);
    process.exit(result.status ?? 1);
  }
  return result.stdout;
}

function linuxLibc() {
  if (process.platform !== 'linux') return undefined;
  const report = process.report && typeof process.report.getReport === 'function'
    ? process.report.getReport()
    : null;
  return report && report.header && report.header.glibcVersionRuntime
    ? 'gnu'
    : 'musl';
}

function platformSuffix() {
  if (process.platform === 'darwin') return `darwin-${process.arch}`;
  if (process.platform === 'linux') return `linux-${process.arch}-${linuxLibc()}`;
  if (process.platform === 'win32') return `win32-${process.arch}-msvc`;
  return `${process.platform}-${process.arch}`;
}

function readPackedManifest(manifest) {
  return JSON.parse(runCapture('tar', ['-xOf', tarballPath(manifest), 'package/package.json']));
}

function assertSameList(label, actual, expected) {
  const actualText = actual.join('\n');
  const expectedText = expected.join('\n');
  if (actualText !== expectedText) {
    throw new Error([
      `${label} mismatch`,
      `actual:\n${actualText}`,
      `expected:\n${expectedText}`,
    ].join('\n'));
  }
}

function verifyTarballCoverage(manifests, packedEntryManifest) {
  for (const { manifest } of manifests) {
    tarballPath(manifest);
  }

  const platformPackageNames = manifests
    .filter(({ dir }) => dir !== 'packages/entry' && dir !== 'packages/loader')
    .map(({ manifest }) => manifest.name)
    .sort();
  const optionalNames = Object.keys(packedEntryManifest.optionalDependencies || {}).sort();
  assertSameList('packed entry optionalDependencies', optionalNames, platformPackageNames);
}

function packageInstallDir(packageName) {
  return path.join(tempRoot, 'node_modules', ...packageName.split('/'));
}

function unpackTarball(manifest) {
  const extractRoot = fs.mkdtempSync(path.join(tempRoot, 'extract-'));
  run('tar', ['-xzf', tarballPath(manifest), '-C', extractRoot]);

  const source = path.join(extractRoot, 'package');
  const destination = packageInstallDir(manifest.name);
  fs.rmSync(destination, { recursive: true, force: true });
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.renameSync(source, destination);
  fs.rmSync(extractRoot, { recursive: true, force: true });
  console.log(`Unpacked ${manifest.name} -> ${path.relative(tempRoot, destination)}`);
}

const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'narb-packed-install-'));
const manifests = packageManifests();
const entryManifest = manifests.find(({ manifest }) => manifest.name === entryPackageName)?.manifest;
const loaderManifest = manifests.find(({ manifest }) => manifest.name === loaderPackageName)?.manifest;
const currentPlatformPackageName = `${entryPackageName}-${platformSuffix()}`;
const currentPlatformManifest = manifests.find(
  ({ manifest }) => manifest.name === currentPlatformPackageName,
)?.manifest;

if (!entryManifest) throw new Error(`missing source manifest for ${entryPackageName}`);
if (!loaderManifest) throw new Error(`missing source manifest for ${loaderPackageName}`);
if (!currentPlatformManifest) {
  throw new Error(`current platform package is not in the release matrix: ${currentPlatformPackageName}`);
}

verifyTarballCoverage(manifests, readPackedManifest(entryManifest));

fs.writeFileSync(
  path.join(tempRoot, 'package.json'),
  `${JSON.stringify({
    name: 'narb-packed-install-check',
    version: '0.0.0',
    private: true,
  }, null, 2)}\n`,
);

console.log(`Verifying packed install in ${tempRoot}`);

// Build a minimal installed tree from local tarballs only. This avoids registry
// access for private optional packages while still running the entry package's
// install lifecycle against the exact packed files.
unpackTarball(loaderManifest);
unpackTarball(currentPlatformManifest);
unpackTarball(entryManifest);

const entryInstallDir = packageInstallDir(entryPackageName);
const entryInstallScript = path.join(entryInstallDir, 'scripts', 'install.js');
const entryPackedManifest = readJson(path.join(entryInstallDir, 'package.json'));
if (entryPackedManifest.scripts?.install !== 'node ./scripts/install.js') {
  throw new Error(`unexpected packed install script: ${entryPackedManifest.scripts?.install}`);
}

run(process.execPath, [entryInstallScript], {
  cwd: entryInstallDir,
  env: { CI: 'true' },
});
run(
  process.execPath,
  ['-e', `
    const addon = require(${JSON.stringify(entryPackageName)});
    const info = addon.getBindingInfo();
    if (!/^optional-package/.test(info.bindingSource)) {
      throw new Error('expected optional package binding, got ' + info.bindingSource);
    }
    addon.requireBuiltin('internal/modules/esm/loader');
    addon.requireBuiltin('internal/modules/cjs/loader');
    console.log(JSON.stringify({
      package: ${JSON.stringify(entryPackageName)},
      bindingSource: info.bindingSource,
      backend: info.backend,
      abi: info.abi
    }, null, 2));
  `],
  {
    cwd: tempRoot,
    env: {
      NARB_DISABLE_LOCAL_BUILD: '1',
    },
  },
);

console.log('Packed install verification passed.');
