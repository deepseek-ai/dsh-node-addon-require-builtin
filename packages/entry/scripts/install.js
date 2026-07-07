'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { toPortablePath } = require('./path-utils.js');

const root = path.resolve(__dirname, '..');
const NAPI_VERSION = '9';

function relativePathForLog(from, to) {
  return toPortablePath(path.relative(from, to));
}

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
    const trace = process.env.DSH_NODE_ADDON_INTERNAL_TRACE
      ? (line) => require('node:fs').writeSync(2, '[dsh-probe] ' + line + '\\n')
      : () => {};
    try {
      trace('validation: requiring addon entry');
      const addon = require(${JSON.stringify(root)});
      const validateInternalModule = (id) => {
        if (typeof addon.isAllowedInternalId !== 'function') {
          console.error('isAllowedInternalId export was not returned');
          process.exit(1);
        }
        if (!addon.isAllowedInternalId(id)) {
          console.error(id + ' is not allowed');
          process.exit(1);
        }
        if (typeof addon.requireBuiltin !== 'function') {
          console.error('requireBuiltin export was not returned');
          process.exit(1);
        }
        const name = id.replace(/^internal\\/modules\\//, '');
        trace('validation: probing ' + name);
        const exports = addon.requireBuiltin(id);
        if (exports == null || typeof exports !== 'object') {
          console.error(name + ' exports were not returned');
          process.exit(1);
        }
        trace('validation: ' + name + ' keys=' + Object.keys(exports).sort().join(','));
        trace('validation: ' + name + ' ok');
      };
      trace('validation: addon entry loaded');
      validateInternalModule('internal/modules/esm/loader');
      validateInternalModule('internal/modules/cjs/loader');
      if (typeof addon.getBindingInfo !== 'function') {
        console.error('getBindingInfo export was not returned');
        process.exit(1);
      }
      trace('validation: ok');
    } catch (error) {
      console.error(error && error.stack ? error.stack : error);
      if (error && error.code) {
        console.error('code:', error.code);
      }
      if (error && error.diagnostics) {
        console.error('diagnostics:', JSON.stringify(error.diagnostics, null, 2));
      }
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

function traceEnabled() {
  const value = process.env.DSH_NODE_ADDON_INTERNAL_TRACE;
  return value != null && value !== '' && value !== '0';
}

function writeValidationOutput(result) {
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
}

function optionalPrebuildWorks() {
  const result = runValidation({
    DSH_NODE_ADDON_INTERNAL_BACKEND: 'auto',
    DSH_NODE_ADDON_INTERNAL_DISABLE_LOCAL_BUILD: '1',
  });
  return result.status === 0;
}

function isNodeExecutableScript(command) {
  const ext = path.extname(command).toLowerCase();
  return ext === '.js' || ext === '.cjs' || ext === '.mjs';
}

function uniqueExistingPaths(candidates, exists) {
  const seen = new Set();
  const found = [];
  for (const candidate of candidates) {
    const resolved = path.resolve(candidate);
    if (seen.has(resolved)) continue;
    seen.add(resolved);
    if (exists(resolved)) found.push(resolved);
  }
  return found;
}

function bundledNodeGypScript(execPath, npmExecPath, exists = fs.existsSync) {
  const candidates = [
    path.join(root, 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
    path.join(root, '..', '..', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
    path.join(root, '..', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
  ];
  if (npmExecPath) {
    const npmExecDir = path.dirname(npmExecPath);
    candidates.push(
      path.join(npmExecDir, 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
      path.join(npmExecDir, '..', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
      path.join(npmExecDir, '..', 'dist', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
      path.join(npmExecDir, '..', '..', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
      path.join(npmExecDir, '..', 'pnpm', 'dist', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
    );
  }

  candidates.push(
    path.join(
      path.dirname(execPath),
      'node_modules',
      'npm',
      'node_modules',
      'node-gyp',
      'bin',
      'node-gyp.js',
    ),
  );

  return uniqueExistingPaths(candidates, exists)[0];
}

function nodeGypInvocationFor(
  platform,
  configuredNodeGyp,
  execPath,
  npmExecPath,
  exists = fs.existsSync,
) {
  if (platform === 'win32') {
    if (configuredNodeGyp && isNodeExecutableScript(configuredNodeGyp)) {
      return {
        command: execPath,
        args: [configuredNodeGyp, 'rebuild'],
        shell: false,
      };
    }
    if (!configuredNodeGyp) {
      const bundledNodeGyp = bundledNodeGypScript(execPath, npmExecPath, exists);
      if (bundledNodeGyp) {
        return {
          command: execPath,
          args: [bundledNodeGyp, 'rebuild'],
          shell: false,
        };
      }
    }
    return {
      command: configuredNodeGyp || 'node-gyp.cmd',
      args: ['rebuild'],
      shell: true,
    };
  }

  return {
    command: configuredNodeGyp || 'node-gyp',
    args: ['rebuild'],
    shell: false,
  };
}

function nodeGypInvocation() {
  return nodeGypInvocationFor(
    process.platform,
    process.env.npm_config_node_gyp,
    process.execPath,
    process.env.npm_execpath,
  );
}

function nodeGypDefines(existingDefines) {
  return [
    existingDefines,
    'enable_lto=false',
    'enable_thin_lto=false',
    'lto_jobs=',
  ].filter(Boolean).join(' ');
}

function nodeGypArgsFor(platform, invocationArgs) {
  const args = [...invocationArgs];
  if (platform !== 'win32') return args;

  const ltoArgs = [
    '--enable-lto=false',
    '--enable-thin-lto=false',
    '--lto-jobs=',
  ];
  const rebuildIndex = args.lastIndexOf('rebuild');
  if (rebuildIndex === -1) return [...args, ...ltoArgs];
  args.splice(rebuildIndex + 1, 0, ...ltoArgs);
  return args;
}

function runNodeGyp() {
  const invocation = nodeGypInvocation();
  const result = spawnSync(invocation.command, nodeGypArgsFor(process.platform, invocation.args), {
    cwd: root,
    stdio: 'inherit',
    shell: invocation.shell,
    env: {
      ...process.env,
      DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
      GYP_DEFINES: nodeGypDefines(process.env.GYP_DEFINES),
      npm_config_enable_lto: 'false',
      npm_config_enable_thin_lto: 'false',
      npm_config_lto_jobs: '',
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
  console.log(`Copied ${relativePathForLog(root, source)} -> ${relativePathForLog(root, target)}`);
}

function validateLocalBuild() {
  const result = runValidation({
    DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
    DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE: '1',
  });
  const shouldEcho = traceEnabled();
  if (shouldEcho) writeValidationOutput(result);
  if (result.status === 0) {
    if (shouldEcho) {
      console.error('[dsh-probe] local build validation: ok');
    }
    return;
  }

  if (!shouldEcho) writeValidationOutput(result);
  console.error(
    `local build validation failed (status=${result.status}, signal=${result.signal || ''})`,
  );
  process.exit(result.status ?? 1);
}

function main() {
  if (optionalPrebuildWorks()) {
    return;
  }

  console.log('node-addon-require-builtin: optional prebuild unavailable, building from source');
  runNodeGyp();
  copyNodeGypOutput();
  validateLocalBuild();
}

module.exports = {
  bundledNodeGypScript,
  nodeGypArgsFor,
  nodeGypDefines,
  nodeGypInvocationFor,
  validationScript,
};

if (require.main === module) {
  main();
}
