import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

// Compiles and runs test/getter_decoder_selftest.cc against the real decoder
// implementation. The decoder is host-architecture independent (it decodes a
// byte buffer), so this exercises every platform's getter shape on one host
// without needing to run on that platform.
const root = path.resolve(__dirname, '..');
const packageRoot = path.join(root, 'packages', 'native');
const nodeInclude = path.join(
  path.dirname(process.execPath),
  '..',
  'include',
  'node',
);
const outDir = path.join(root, 'build');
const outBin = path.join(outDir, 'getter_decoder_selftest');

const compiler = process.env.CXX || 'c++';
const args = [
  '-std=c++17',
  '-Wall',
  '-Wextra',
  '-fno-exceptions',
  '-I',
  nodeInclude,
  '-I',
  path.join(packageRoot, 'src'),
  '-DNAPI_VERSION=9',
  '-DNARB_BACKEND=1',
  '-DNARB_PRODUCT=2',
  path.join(packageRoot, 'src', 'runtime_probe', 'getter_decoder.cc'),
  path.join(root, 'test', 'getter_decoder_selftest.cc'),
  '-o',
  outBin,
];

fs.mkdirSync(outDir, { recursive: true });

const build = spawnSync(compiler, args, { cwd: root, stdio: 'inherit' });
if (build.status !== 0) process.exit(build.status ?? 1);

const run = spawnSync(outBin, [], { cwd: root, stdio: 'inherit' });
process.exit(run.status ?? 1);
