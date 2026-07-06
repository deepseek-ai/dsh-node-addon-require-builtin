'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const root = path.resolve(__dirname, '..');
const NAPI_VERSION = '9';

function linuxLibc() {
  if (process.platform !== 'linux') return undefined;
  const report = process.report && typeof process.report.getReport === 'function'
    ? process.report.getReport()
    : null;
  return report && report.header && report.header.glibcVersionRuntime
    ? 'glibc'
    : 'musl';
}

function platformSuffix() {
  if (process.platform === 'darwin') return `darwin-${process.arch}`;
  if (process.platform === 'linux') {
    const libc = linuxLibc() === 'glibc' ? 'gnu' : 'musl';
    return `linux-${process.arch}-${libc}`;
  }
  if (process.platform === 'win32') return `win32-${process.arch}-msvc`;
  return `${process.platform}-${process.arch}`;
}

function localBuildRelativePath() {
  return path.join(
    'build',
    'napi',
    `napi-v${NAPI_VERSION}-${platformSuffix()}`,
    'internal_require.node',
  );
}

function validationScript() {
  return `
    const addon = require(${JSON.stringify(root)});
    const cjsLoader = addon.getModulesCjsLoader();
    const esmLoader = addon.getModulesEsmLoader();
    if (cjsLoader == null || esmLoader == null) {
      console.error('internal loader exports were not returned');
      process.exit(1);
    }
  `;
}

function runValidation(extraEnv) {
  return spawnSync(process.execPath, ['-e', validationScript()], {
    cwd: root,
    encoding: 'utf8',
    env: {
      ...process.env,
      ...extraEnv,
    },
  });
}

function optionalPrebuildWorks() {
  const result = runValidation({
    DSH_NODE_ADDON_INTERNAL_BACKEND: 'auto',
    DSH_NODE_ADDON_INTERNAL_DISABLE_LOCAL_BUILD: '1',
  });
  return result.status === 0;
}

function nodeGypCommand() {
  if (process.env.npm_config_node_gyp) {
    return process.env.npm_config_node_gyp;
  }
  return process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp';
}

function runNodeGyp() {
  const result = spawnSync(nodeGypCommand(), ['rebuild'], {
    cwd: root,
    stdio: 'inherit',
    env: {
      ...process.env,
      DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function copyNodeGypOutput() {
  const source = path.join(root, 'build', 'Release', 'internal_require.node');
  const target = path.join(root, localBuildRelativePath());

  if (!fs.existsSync(source)) {
    throw new Error(`node-gyp output not found: ${source}`);
  }

  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.copyFileSync(source, target);
  console.log(`Copied ${path.relative(root, source)} -> ${path.relative(root, target)}`);
}

function validateLocalBuild() {
  const result = runValidation({
    DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
    DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE: '1',
  });
  if (result.status === 0) return;

  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  process.exit(result.status ?? 1);
}

function main() {
  if (optionalPrebuildWorks()) {
    return;
  }

  console.log('@deepseek-ai/dsh-node-addon-internal: optional prebuild unavailable, building from source');
  runNodeGyp();
  copyNodeGypOutput();
  validateLocalBuild();
}

main();
