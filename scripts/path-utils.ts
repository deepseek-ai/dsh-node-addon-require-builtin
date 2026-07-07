import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

export function splitPathList(value: string | undefined): string[] {
  return value ? value.split(path.delimiter).filter(Boolean) : [];
}

export function joinPathList(values: string[]): string {
  return values.join(path.delimiter);
}

export function toPortablePath(value: string): string {
  return value.replace(/\\/g, '/');
}

export function relativePathForLog(from: string, to: string): string {
  return toPortablePath(path.relative(from, to));
}

export function pathForCliArg(value: string): string {
  return process.platform === 'win32' ? toPortablePath(value) : value;
}

export function writeSimpleGithubEnv(file: string, name: string, value: string): void {
  if (value.includes('\n') || value.includes('\r')) {
    throw new Error(`${name} contains a newline and cannot be written as a simple GitHub env value`);
  }
  fs.appendFileSync(file, `${name}=${value}${os.EOL}`);
}
