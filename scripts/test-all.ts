import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const root = path.resolve(__dirname, '..');
const majors = [20, 22, 24, 26];

function nvmVersionsDir(): string {
  return path.join(process.env.NVM_DIR || path.join(process.env.HOME || '', '.nvm'), 'versions', 'node');
}

function discoverNodeBins(): string[] {
  const explicit = process.env.NODE_TEST_BINS;
  if (explicit) {
    return explicit.split(path.delimiter).filter(Boolean);
  }

  const dir = nvmVersionsDir();
  if (!fs.existsSync(dir)) return [];

  const bins: string[] = [];
  for (const major of majors) {
    const candidates = fs
      .readdirSync(dir)
      .filter((entry) => entry.startsWith(`v${major}.`))
      .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }));
    if (candidates.length > 0) {
      bins.push(path.join(dir, candidates[0], 'bin', 'node'));
    }
  }
  return bins;
}

function run(node: string, args: string[]): void {
  const label = `${path.basename(path.dirname(path.dirname(node)))} ${args.join(' ')}`;
  console.log(`\n== ${label}`);
  const result = spawnSync(node, args, {
    cwd: root,
    stdio: 'inherit',
    env: {
      ...process.env,
      DSH_NODE_ADDON_INTERNAL_BACKEND: 'napi',
      DSH_NODE_ADDON_INTERNAL_EXPECTED_BACKEND: 'napi',
      DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE: '1',
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function main(): void {
  const bins = discoverNodeBins();
  const versions = new Map<number, { node: string; version: string }>();
  for (const node of bins) {
    const result = spawnSync(node, ['-p', 'process.versions.node'], {
      cwd: root,
      encoding: 'utf8',
    });
    if (result.status !== 0) continue;
    const version = result.stdout.trim();
    versions.set(Number(version.split('.')[0]), { node, version });
  }

  const missing = majors.filter((major) => !versions.has(major));
  if (missing.length > 0) {
    throw new Error(`missing local Node majors: ${missing.join(', ')}`);
  }

  const buildRuntime = versions.get(20);
  if (!buildRuntime) throw new Error('missing Node 20 build runtime');
  console.log(`\n## Build once with Node ${buildRuntime.version}`);
  run(buildRuntime.node, ['--import', 'tsx', './scripts/build.ts']);

  for (const major of majors) {
    const runtime = versions.get(major);
    if (!runtime) throw new Error(`missing Node ${major} runtime`);
    console.log(`\n## Test with Node ${runtime.version}`);
    run(runtime.node, ['./test/backend.test.js']);
  }
}

main();
