#!/usr/bin/env node

// Shared package-discovery helpers for the release/CI scripts. The workspace
// holds one shared loader plus two product families, each with its own entry
// package and per-platform optional packages:
//
//   packages/loader                         (shared, published)
//   packages/<family>/entry                 (published)
//   packages/<family>/<platform>            (published, has prebuilds.json)
//
// Everything that used to assume a flat `packages/*` layout goes through the
// helpers here so the family dimension lives in exactly one place.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const root = fileURLToPath(new URL('..', import.meta.url));
export const packagesRoot = path.join(root, 'packages');

// Published families, in the order their packages should be released.
export const FAMILIES = ['require-builtin', 'internal-loader'];

export const LOADER_DIR = 'packages/loader';

export function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

// Platform package directories for one family, sorted, as repo-relative paths.
// A platform package is any subdirectory (other than `entry`) that carries a
// prebuilds.json.
export function platformDirsFor(family) {
  const familyRoot = path.join(packagesRoot, family);
  if (!fs.existsSync(familyRoot)) return [];
  return fs.readdirSync(familyRoot)
    .filter((name) => name !== 'entry')
    .filter((name) => fs.existsSync(path.join(familyRoot, name, 'prebuilds.json')))
    .sort()
    .map((name) => path.join('packages', family, name));
}

export function entryDirFor(family) {
  return path.join('packages', family, 'entry');
}

// All publishable package directories in dependency/publish order: the shared
// loader first (everything depends on it), then each family's platform packages
// followed by its entry package (the entry's optionalDependencies point at the
// platform packages, so they must publish first).
export function allPublishDirs() {
  const dirs = [LOADER_DIR];
  for (const family of FAMILIES) {
    dirs.push(...platformDirsFor(family));
    dirs.push(entryDirFor(family));
  }
  return dirs;
}

// Every package.json that participates in version bumping: the publishable set
// plus the root workspace manifest and the hmr-comparison helper.
export function versionedFiles() {
  return [
    'package.json',
    'hmr-comparison/package.json',
    ...allPublishDirs().map((dir) => `${dir}/package.json`),
  ].filter((file) => fs.existsSync(path.join(root, file)));
}

// Flat list of { family, platform, dir } for every platform package across all
// families — the release/CI build matrix and prebuild assembly iterate this.
export function allPlatformPackages() {
  const result = [];
  for (const family of FAMILIES) {
    for (const dir of platformDirsFor(family)) {
      result.push({ family, platform: path.basename(dir), dir });
    }
  }
  return result;
}
