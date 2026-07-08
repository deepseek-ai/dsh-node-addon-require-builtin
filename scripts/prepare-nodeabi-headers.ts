import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { gunzipSync } from 'node:zlib';
import {
  joinPathList,
  writeSimpleGithubEnv,
} from './path-utils.js';

const root = path.resolve(__dirname, '..');
const cacheRoot = process.env.NARB_HEADERS_CACHE ||
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

function readTarString(buffer: Buffer, start: number, length: number): string {
  const end = buffer.indexOf(0, start);
  const sliceEnd = end >= start && end < start + length ? end : start + length;
  return buffer.toString('utf8', start, sliceEnd);
}

function readTarOctal(buffer: Buffer, start: number, length: number): number {
  const text = readTarString(buffer, start, length).trim();
  return text ? Number.parseInt(text, 8) : 0;
}

function isZeroBlock(buffer: Buffer, start: number): boolean {
  for (let index = start; index < start + 512; index += 1) {
    if (buffer[index] !== 0) return false;
  }
  return true;
}

function strippedEntryParts(entryName: string): string[] | null {
  const parts = entryName
    .replace(/\\/g, '/')
    .split('/')
    .filter((part) => part.length > 0 && part !== '.');

  if (parts.some((part) => part === '..')) {
    throw new Error(`unsafe path in Node.js headers archive: ${entryName}`);
  }

  const stripped = parts.slice(1);
  return stripped.length > 0 ? stripped : null;
}

function destinationPath(rootDir: string, entryName: string): string | null {
  const parts = strippedEntryParts(entryName);
  if (!parts) return null;

  const rootPath = path.resolve(rootDir);
  const target = path.resolve(rootPath, ...parts);
  const relative = path.relative(rootPath, target);
  if (relative === '' || relative.startsWith('..') || path.isAbsolute(relative)) {
    throw new Error(`unsafe path in Node.js headers archive: ${entryName}`);
  }
  return target;
}

function extractNodeHeadersTarball(tarball: string, destination: string): void {
  const data = gunzipSync(fs.readFileSync(tarball));
  let longName: string | undefined;

  for (let offset = 0; offset + 512 <= data.length;) {
    if (isZeroBlock(data, offset)) return;

    const name = readTarString(data, offset, 100);
    const size = readTarOctal(data, offset + 124, 12);
    const type = readTarString(data, offset + 156, 1) || '0';
    const prefix = readTarString(data, offset + 345, 155);
    const entryName = longName || (prefix ? `${prefix}/${name}` : name);
    longName = undefined;

    const contentStart = offset + 512;
    const contentEnd = contentStart + size;
    const nextOffset = contentStart + Math.ceil(size / 512) * 512;

    if (type === 'L') {
      longName = data.toString('utf8', contentStart, contentEnd).replace(/\0.*$/s, '');
      offset = nextOffset;
      continue;
    }
    if (type === 'x' || type === 'g') {
      offset = nextOffset;
      continue;
    }

    const target = destinationPath(destination, entryName);
    if (target) {
      if (type === '5') {
        fs.mkdirSync(target, { recursive: true });
      } else if (type === '0' || type === '') {
        fs.mkdirSync(path.dirname(target), { recursive: true });
        fs.writeFileSync(target, data.subarray(contentStart, contentEnd));
      } else {
        throw new Error(`unsupported entry type in Node.js headers archive: ${type} ${entryName}`);
      }
    }

    offset = nextOffset;
  }

  throw new Error(`invalid tar archive: ${tarball}`);
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
  extractNodeHeadersTarball(tarball, tmp);
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

function githubEnvPath(): string | undefined {
  const index = process.argv.findIndex((arg) => arg === '--github-env');
  if (index < 0) return undefined;

  const explicitPath = process.argv[index + 1];
  if (explicitPath && !explicitPath.startsWith('-')) {
    return explicitPath;
  }

  if (!process.env.GITHUB_ENV) {
    throw new Error('--github-env requires GITHUB_ENV or an explicit file path');
  }
  return process.env.GITHUB_ENV;
}

function printShellEnv(value: string): void {
  console.log(`NODE_JS_PUBLIC_INCLUDE_DIRS=${shellQuote(value)}`);
  console.log('export NODE_JS_PUBLIC_INCLUDE_DIRS');
}

function main(): void {
  const version = requestedVersion();
  const dir = ensureHeaders(version);
  const value = joinPathList(includeDirs(dir));
  const envPath = githubEnvPath();
  if (envPath) {
    writeSimpleGithubEnv(envPath, 'NODE_JS_PUBLIC_INCLUDE_DIRS', value);
    console.log(`NODE_JS_PUBLIC_INCLUDE_DIRS written to ${envPath}`);
    return;
  }

  printShellEnv(value);
}

main();
