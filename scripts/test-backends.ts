import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import path from 'node:path';

const root = path.resolve(__dirname, '..');

interface ProbeSummary {
  backend: string;
  abi: string;
  esm_loader_type: string;
  cjs_loader_type: string;
}

function run(args: string[], extraEnv: Record<string, string> = {}): void {
  const result = spawnSync(process.execPath, args, {
    cwd: root,
    stdio: 'inherit',
    env: {
      ...process.env,
      ...extraEnv,
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) process.exit(result.status ?? 1);
}

function captureProbe(backend: string): ProbeSummary {
  const script = `
    const addon = require('./packages/entry');
    const info = addon.getBindingInfo();
    const esmLoader = addon.getModulesEsmLoader();
    const cjsLoader = addon.getModulesCjsLoader();
    console.log(JSON.stringify({
      backend: info.backend,
      abi: info.abi,
      esm_loader_type: typeof esmLoader,
      cjs_loader_type: typeof cjsLoader,
    }));
  `;

  const result = spawnSync(process.execPath, ['-e', script], {
    cwd: root,
    encoding: 'utf8',
    env: {
      ...process.env,
      DSH_NODE_ADDON_INTERNAL_BACKEND: backend,
      DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE: '1',
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.stdout.write(result.stdout);
    process.stderr.write(result.stderr);
    process.exit(result.status ?? 1);
  }
  return JSON.parse(result.stdout) as ProbeSummary;
}

if (!process.env.NODE_JS_PUBLIC_INCLUDE_DIRS) {
  throw new Error('test:backends requires NODE_JS_PUBLIC_INCLUDE_DIRS');
}

run(['--import', 'tsx', './scripts/build.ts'], {
  DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
});
run(['--import', 'tsx', './scripts/build.ts'], {
  DSH_NODE_ADDON_INTERNAL_BACKEND: 'nodeabi',
});

const napi = captureProbe('napi');
const nodeabi = captureProbe('nodeabi');

assert.equal(napi.backend, 'napi');
assert.equal(nodeabi.backend, 'nodeabi');
assert.equal(napi.esm_loader_type, nodeabi.esm_loader_type);
assert.equal(napi.cjs_loader_type, nodeabi.cjs_loader_type);

console.log(JSON.stringify({ napi, nodeabi }, null, 2));
