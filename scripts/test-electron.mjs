#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

import { electronReleaseCatalog } from './electron-releases.mjs';

const require = createRequire(import.meta.url);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const manifest = JSON.parse(
  fs.readFileSync(path.join(root, 'package.json'), 'utf8'),
);

class ElectronTestRunner {
  constructor() {
    this.releases = electronReleaseCatalog.select(
      process.env.NARB_ELECTRON_VERSIONS ||
        process.env.NARB_EXPECTED_ELECTRON_VERSION,
    );
    this.expectedArch = process.env.NARB_EXPECTED_ARCH || process.arch;
    this.app = path.join(root, 'test', 'electron-main.js');
    this.families = ['require-builtin', 'internal-loader'];
  }

  executableFor(release) {
    if (process.env.ELECTRON_EXECUTABLE) {
      if (this.releases.length !== 1) {
        throw new Error(
          'ELECTRON_EXECUTABLE requires exactly one NARB_ELECTRON_VERSIONS entry',
        );
      }
      return process.env.ELECTRON_EXECUTABLE;
    }
    release.assertDependency(manifest);
    return require(release.packageName);
  }

  prepare() {
    for (const release of this.releases) {
      const executable = this.executableFor(release);
      if (!fs.existsSync(executable)) {
        throw new Error(`Electron executable not found: ${executable}`);
      }
      console.log(
        `Prepared Electron ${release.version} ${this.expectedArch}: ${executable}`,
      );
    }
  }

  runFamily(release, executable, family) {
    console.log(
      `Running Electron ${release.version} ${this.expectedArch}: ${family}`,
    );
    const env = {
      ...process.env,
      NARB_PRODUCT: family,
      NARB_EXPECTED_PRODUCT: family,
      NARB_BACKEND: 'napi',
      NARB_EXPECTED_BACKEND: 'napi',
      NARB_EXPECTED_ELECTRON_VERSION: release.version,
      NARB_EXPECTED_ARCH: this.expectedArch,
    };
    delete env.ELECTRON_RUN_AS_NODE;

    const isLinux = process.platform === 'linux';
    const command = isLinux ? 'xvfb-run' : executable;
    const args = isLinux
      ? ['-a', executable, '--no-sandbox', this.app]
      : [this.app];
    const result = spawnSync(command, args, {
      cwd: root,
      env,
      stdio: 'inherit',
    });
    if (result.error) throw result.error;
    if (result.status !== 0) process.exit(result.status ?? 1);
  }

  run() {
    for (const release of this.releases) {
      const executable = this.executableFor(release);
      if (!fs.existsSync(executable)) {
        throw new Error(`Electron executable not found: ${executable}`);
      }
      for (const family of this.families) {
        this.runFamily(release, executable, family);
      }
    }
  }
}

const runner = new ElectronTestRunner();
if (process.argv.includes('--prepare-only')) {
  runner.prepare();
} else {
  runner.run();
}
