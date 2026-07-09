#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';

import { allPublishDirs, readJson, root } from './packages.mjs';

function verifyVersions() {
  const packages = allPublishDirs().map((dir) => ({
    dir,
    manifest: readJson(path.join(root, dir, 'package.json')),
  }));
  const versions = new Set(packages.map((pkg) => pkg.manifest.version));
  if (versions.size !== 1) {
    throw new Error([
      'published package versions must match:',
      ...packages.map((pkg) => `${pkg.dir}: ${pkg.manifest.version}`),
    ].join('\n'));
  }

  const version = packages[0].manifest.version;
  const ref = process.env.GITHUB_REF || '';
  const publish = process.env.RELEASE_PUBLISH === 'true';
  if (publish && !ref.startsWith('refs/tags/v')) {
    throw new Error('publishing requires running the workflow from a v* tag');
  }
  if (ref.startsWith('refs/tags/v')) {
    const tagVersion = ref.slice('refs/tags/v'.length);
    if (tagVersion !== version) {
      throw new Error(`tag/version mismatch: tag v${tagVersion}, packages ${version}`);
    }
  }

  console.log(`Verified release version ${version}`);
}

function verifyPrebuilds() {
  for (const dir of allPublishDirs()) {
    const prebuildsFile = path.join(root, dir, 'prebuilds.json');
    if (!fs.existsSync(prebuildsFile)) continue;

    const manifest = readJson(prebuildsFile);
    const expected = manifest.binaries.map((binary) => path.basename(binary.path)).sort();
    const prebuiltDir = path.join(root, dir, 'prebuilt');
    const actual = fs.existsSync(prebuiltDir)
      ? fs.readdirSync(prebuiltDir).filter((name) => name.endsWith('.node')).sort()
      : [];
    const missing = expected.filter((name) => !actual.includes(name));
    const extra = actual.filter((name) => !expected.includes(name));
    if (missing.length || extra.length) {
      throw new Error([
        `prebuild mismatch for ${dir}`,
        missing.length ? `missing: ${missing.join(', ')}` : '',
        extra.length ? `extra: ${extra.join(', ')}` : '',
      ].filter(Boolean).join('\n'));
    }

    console.log(`Verified ${dir}: ${actual.length} prebuilds`);
  }
}

verifyVersions();
if (process.argv.includes('--prebuilds')) {
  verifyPrebuilds();
}
