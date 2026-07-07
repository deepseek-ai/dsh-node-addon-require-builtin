'use strict';

const assert = require('node:assert/strict');
const addon = require('../packages/entry');
const { toPortablePath } = require('../packages/entry/scripts/path-utils.js');

function sortedKeys(value) {
  return Object.keys(value).sort();
}

const info = addon.getBindingInfo();
assert.equal(addon.getBindingInfo(), info);

const requestedBackend = process.env.DSH_NODE_ADDON_INTERNAL_BACKEND;
const expectedBackend = process.env.DSH_NODE_ADDON_INTERNAL_EXPECTED_BACKEND ||
  (requestedBackend === 'nodeabi' || requestedBackend === 'napi'
    ? requestedBackend
    : info.backend);
const expectedMode = expectedBackend;
const expectedAbi = expectedBackend === 'nodeabi'
  ? `node-v${process.versions.modules}`
  : 'napi-v9';
const platformSuffix = info.platformPackageSuffix;
const expectedOptionalBinary = expectedBackend === 'nodeabi'
  ? `prebuilt/${platformSuffix}-nodeabi-v${process.versions.modules}.node`
  : `prebuilt/${platformSuffix}-napi-v9.node`;
const expectedLocalBuildPath = expectedBackend === 'nodeabi'
  ? new RegExp(`build/nodeabi/node-v[0-9]+-${platformSuffix}/internal_require\\.node$`)
  : new RegExp(`build/napi/napi-v9-${platformSuffix}/internal_require\\.node$`);

assert.equal(
  info.optionalPackageName,
  `node-addon-require-builtin-${platformSuffix}`,
);
assert.equal(typeof platformSuffix, 'string');
assert.match(info.bindingPath, /\.node$/);
assert.match(toPortablePath(info.localBindingPath), expectedLocalBuildPath);
assert.equal(toPortablePath(info.optionalBinaryRelativePath), expectedOptionalBinary);

if (process.env.DSH_NODE_ADDON_INTERNAL_DISABLE_LOCAL_BUILD === '1') {
  assert.match(info.bindingSource, /^optional-package/);
} else {
  assert.match(
    info.bindingSource,
    /^(optional-package|optional-package-workspace|local-build)$/,
  );
}

assert.equal(
  process.execArgv.includes('--expose-internals'),
  false,
  'tests must run without --expose-internals',
);
assert.throws(
  () => require('internal/modules/esm/loader'),
  /Cannot find module|No such built-in module/,
  'public require should not load internal modules without --expose-internals',
);

assert.equal(info.mode, expectedMode);
assert.equal(info.backend, expectedBackend);
assert.equal(info.abi, expectedAbi);

const nativeBinding = require(info.bindingPath);
assert.equal(typeof nativeBinding.getNativeBindingInfo, 'function');
assert.equal(typeof nativeBinding.requireBuiltin, 'function');
assert.equal(typeof nativeBinding.isAllowedInternalId, 'function');
assert.deepEqual(nativeBinding.getNativeBindingInfo(), {
  mode: expectedMode,
  backend: expectedBackend,
  abi: expectedAbi,
});

assert.equal(typeof addon.requireBuiltin, 'function');
assert.equal(typeof addon.isAllowedInternalId, 'function');
assert.equal(addon.isAllowedInternalId('internal/modules/esm/loader'), true);
assert.equal(addon.isAllowedInternalId('internal/modules/cjs/loader'), true);
assert.equal(addon.isAllowedInternalId('internal/bootstrap/realm'), false);
assert.equal(addon.isAllowedInternalId(''), false);
assert.throws(() => addon.requireBuiltin(), /moduleId must be a string/);
assert.throws(
  () => addon.requireBuiltin('internal/bootstrap/realm'),
  (error) => {
    assert.equal(error.code, 'Unsupported/disallowed-target');
    assert.equal(error.diagnostics.target, 'internal/bootstrap/realm');
    assert.match(error.message, /moduleId must be one of:/);
    return true;
  },
);

const esmLoader = addon.requireBuiltin('internal/modules/esm/loader');
assert.equal(typeof esmLoader, 'object');
console.log(`esm_loader_keys=${sortedKeys(esmLoader).join(',')}`);

const cjsLoader = addon.requireBuiltin('internal/modules/cjs/loader');
assert.equal(typeof cjsLoader, 'object');
console.log(`cjs_loader_keys=${sortedKeys(cjsLoader).join(',')}`);

console.log(
  JSON.stringify(
    {
      node: process.version,
      napi: process.versions.napi,
      binding_source: info.bindingSource,
      mode: info.mode,
      backend: info.backend,
      abi: info.abi,
    },
    null,
    2,
  ),
);
