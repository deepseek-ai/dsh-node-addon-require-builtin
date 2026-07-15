#!/usr/bin/env node

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import {
  allPublishDirs,
  entryDirFor,
  FAMILIES,
  LOADER_DIR,
  platformDirsFor,
  readJson,
  root,
} from './packages.mjs';

const tarballDir = path.resolve(process.argv[2] || path.join(root, 'dist', 'npm'));
const loaderManifest = readJson(path.join(root, LOADER_DIR, 'package.json'));

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

// Confirm every package in the release has a tarball, and that each family's
// packed entry lists exactly its own platform packages as optionalDependencies.
function verifyTarballCoverage() {
  for (const dir of allPublishDirs()) {
    tarballPath(readJson(path.join(root, dir, 'package.json')));
  }

  for (const family of FAMILIES) {
    const entryManifest = readJson(path.join(root, entryDirFor(family), 'package.json'));
    const platformNames = platformDirsFor(family)
      .map((dir) => readJson(path.join(root, dir, 'package.json')).name)
      .sort();
    const packedEntry = readPackedManifest(entryManifest);
    const optionalNames = Object.keys(packedEntry.optionalDependencies || {}).sort();
    assertSameList(`packed ${family} entry optionalDependencies`, optionalNames, platformNames);
  }
}

// Build a minimal installed tree from local tarballs only (no registry access
// for the private optional packages) and run a runtime smoke test against the
// exact packed files. The entry package no longer ships an install lifecycle
// script, so the fail-closed path is exercised at require() time below.
function verifyFamily(family) {
  const entryManifest = readJson(path.join(root, entryDirFor(family), 'package.json'));
  const entryPackageName = entryManifest.name;
  const currentPlatformPackageName = `${entryPackageName}-${platformSuffix()}`;
  const currentPlatformDir = platformDirsFor(family).find(
    (dir) => readJson(path.join(root, dir, 'package.json')).name === currentPlatformPackageName,
  );
  if (!currentPlatformDir) {
    throw new Error(`current platform package is not in the ${family} matrix: ${currentPlatformPackageName}`);
  }
  const currentPlatformManifest = readJson(path.join(root, currentPlatformDir, 'package.json'));

  console.log(`\n== Verifying packed install for ${entryPackageName} ==`);
  unpackTarball(currentPlatformManifest);
  unpackTarball(entryManifest);

  const entryInstallDir = packageInstallDir(entryPackageName);
  const entryPackedManifest = readJson(path.join(entryInstallDir, 'package.json'));
  if (entryPackedManifest.scripts?.install !== undefined) {
    throw new Error(
      `packed entry unexpectedly ships an install script: ${entryPackedManifest.scripts.install}`,
    );
  }

  // With the current platform's optional package removed and local source
  // builds disabled, loading the entry must fail closed at require() time.
  const optionalPackageDir = packageInstallDir(currentPlatformPackageName);
  const disabledOptionalPackageDir = `${optionalPackageDir}.disabled`;
  fs.renameSync(optionalPackageDir, disabledOptionalPackageDir);
  try {
    const result = spawnSync(
      process.execPath,
      ['-e', `require(${JSON.stringify(entryPackageName)});`],
      {
        cwd: tempRoot,
        encoding: 'utf8',
        env: {
          ...process.env,
          NARB_DISABLE_LOCAL_BUILD: '1',
        },
      },
    );
    const output = `${result.stdout || ''}${result.stderr || ''}`;
    if (result.status === 0) {
      throw new Error('packed entry loaded without current platform optional package');
    }
    if (!output.includes('No usable native binding found')) {
      throw new Error(`packed entry did not fail closed with the loader error:\n${output}`);
    }
  } finally {
    fs.renameSync(disabledOptionalPackageDir, optionalPackageDir);
  }

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
        product: info.product,
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
}

const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'narb-packed-install-'));

verifyTarballCoverage();

fs.writeFileSync(
  path.join(tempRoot, 'package.json'),
  `${JSON.stringify({
    name: 'narb-packed-install-check',
    version: '0.0.0',
    private: true,
  }, null, 2)}\n`,
);

console.log(`Verifying packed install in ${tempRoot}`);

// The shared loader is unpacked once; both families resolve it by name.
unpackTarball(loaderManifest);

for (const family of FAMILIES) {
  verifyFamily(family);
}

console.log('\nPacked install verification passed.');
