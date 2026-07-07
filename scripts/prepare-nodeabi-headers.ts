import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const root = path.resolve(__dirname, '..');
const cacheRoot = process.env.DSH_NODE_ADDON_INTERNAL_HEADERS_CACHE ||
  process.env.NODE_HEADERS_CACHE ||
  path.join(root, '.cache', 'node-headers');
const baseUrl = process.env.NODE_DIST_BASE_URL ||
  process.env.NODE_MIRROR ||
  'https://nodejs.org/download/release';

function normalizeVersion(input: string): string {
  const version = input.trim().replace(/^v/, '');
  if (!/^\d+\.\d+\.\d+$/.test(version)) {
    throw new Error(`invalid Node.js version: ${input}`);
  }
  return version;
}

function requestedVersion(): string {
  const index = process.argv.findIndex((arg) => arg === '--version' || arg === '-v');
  if (index >= 0) {
    const value = process.argv[index + 1];
    if (!value) throw new Error(`${process.argv[index]} expects a version`);
    return normalizeVersion(value);
  }
  return normalizeVersion(process.versions.node);
}

function run(command: string, args: string[]): void {
  const result = spawnSync(command, args, {
    cwd: root,
    stdio: 'inherit',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function headersUrl(version: string): string {
  return `${baseUrl.replace(/\/$/, '')}/v${version}/node-v${version}-headers.tar.gz`;
}

function headersDir(version: string): string {
  return path.join(cacheRoot, `v${version}`);
}

function includeDirs(dir: string): string[] {
  return [path.join(dir, 'include', 'node')];
}

function tarExtractArgs(tarball: string, destination: string): string[] {
  const args = ['-xzf', tarball, '-C', destination, '--strip-components=1'];
  if (process.platform === 'win32') {
    // GNU tar treats drive-letter paths like D:\... as remote archives unless
    // forced local. GitHub Windows runners hit this path via Git Bash tar.
    return ['--force-local', ...args];
  }
  return args;
}

function ensureHeaders(version: string): string {
  const dir = headersDir(version);
  if (fs.existsSync(path.join(dir, 'include', 'node', 'node.h'))) {
    return dir;
  }

  fs.mkdirSync(cacheRoot, { recursive: true });
  const tarball = path.join(cacheRoot, `node-v${version}-headers.tar.gz`);
  if (!fs.existsSync(tarball)) {
    run('curl', ['-L', headersUrl(version), '-o', tarball]);
  }

  const tmp = path.join(cacheRoot, `.extract-v${version}`);
  fs.rmSync(tmp, { recursive: true, force: true });
  fs.mkdirSync(tmp, { recursive: true });
  run('tar', tarExtractArgs(tarball, tmp));
  fs.rmSync(dir, { recursive: true, force: true });
  fs.renameSync(tmp, dir);

  if (!fs.existsSync(path.join(dir, 'include', 'node', 'node.h'))) {
    throw new Error(`Node.js headers archive did not contain include/node/node.h: ${tarball}`);
  }

  return dir;
}

function shellQuote(value: string): string {
  return `'${value.replace(/'/g, `'\\''`)}'`;
}

function main(): void {
  const version = requestedVersion();
  const dir = ensureHeaders(version);
  const value = includeDirs(dir).join(path.delimiter);
  console.log(`NODE_JS_PUBLIC_INCLUDE_DIRS=${shellQuote(value)}`);
  console.log(`export NODE_JS_PUBLIC_INCLUDE_DIRS`);
}

main();
