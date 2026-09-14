'use strict';

const assert = require('node:assert/strict');
const { app } = require('electron');

async function main() {
  await app.whenReady();

  console.log(JSON.stringify({
    electron: process.versions.electron,
    node: process.versions.node,
    v8: process.versions.v8,
    modules: process.versions.modules,
    napi: process.versions.napi,
    platform: process.platform,
    arch: process.arch,
  }, null, 2));

  assert.equal(
    process.versions.electron,
    process.env.NARB_EXPECTED_ELECTRON_VERSION,
    'Electron version must match the pinned test version',
  );
  if (process.env.NARB_EXPECTED_ARCH) {
    assert.equal(
      process.arch,
      process.env.NARB_EXPECTED_ARCH,
      'Electron architecture must match the native CI runner',
    );
  }

  require('./backend.test.js');
}

main().then(
  () => app.exit(0),
  (error) => {
    console.error(error);
    app.exit(1);
  },
);
