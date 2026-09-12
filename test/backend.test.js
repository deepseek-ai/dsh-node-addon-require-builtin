'use strict';

const assert = require('node:assert/strict');
const {
  toPortablePath,
  productConfig,
  entryPackagePath,
} = require('./product.js');

const { product, entryPackageName, enforcesWhitelist } = productConfig();
const addon = require(entryPackagePath());

function sortedKeys(value) {
  return Object.keys(value).sort();
}

const info = addon.getBindingInfo();
assert.equal(addon.getBindingInfo(), info);

const requestedBackend = process.env.NARB_BACKEND;
const expectedBackend = process.env.NARB_EXPECTED_BACKEND ||
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
  ? new RegExp(`build/nodeabi/node-v[0-9]+-${platformSuffix}/require_builtin\\.node$`)
  : new RegExp(`build/napi/napi-v9-${platformSuffix}/require_builtin\\.node$`);

assert.equal(info.product, product);
assert.equal(
  info.optionalPackageName,
  `${entryPackageName}-${platformSuffix}`,
);
assert.equal(typeof platformSuffix, 'string');
assert.match(info.bindingPath, /\.node$/);
assert.match(toPortablePath(info.localBindingPath), expectedLocalBuildPath);
assert.equal(toPortablePath(info.optionalBinaryRelativePath), expectedOptionalBinary);

if (process.env.NARB_DISABLE_LOCAL_BUILD === '1') {
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
  product,
  backend: expectedBackend,
  abi: expectedAbi,
});

assert.equal(typeof addon.requireBuiltin, 'function');
assert.equal(typeof addon.isAllowedInternalId, 'function');
assert.equal(addon.isAllowedInternalId('internal/modules/esm/loader'), true);
assert.equal(addon.isAllowedInternalId('internal/modules/cjs/loader'), true);
assert.throws(() => addon.requireBuiltin(), /moduleId must be a string/);

if (enforcesWhitelist) {
  // internal-loader restricts ids to the cjs/esm loader allow-list.
  assert.equal(addon.isAllowedInternalId('internal/bootstrap/realm'), false);
  assert.equal(addon.isAllowedInternalId(''), false);
  assert.throws(
    () => addon.requireBuiltin('internal/bootstrap/realm'),
    (error) => {
      assert.equal(error.code, 'Unsupported/disallowed-target');
      assert.equal(error.diagnostics.target, 'internal/bootstrap/realm');
      assert.match(error.message, /moduleId must be one of:/);
      return true;
    },
  );
} else {
  // require-builtin does not restrict ids: isAllowedInternalId is always true
  // and requireBuiltin forwards any id to Node's builtin require.
  assert.equal(addon.isAllowedInternalId('internal/bootstrap/realm'), true);
  assert.equal(addon.isAllowedInternalId(''), true);
  const realm = addon.requireBuiltin('internal/bootstrap/realm');
  assert.equal(typeof realm, 'object');
  console.log(`realm_keys=${sortedKeys(realm).join(',')}`);
}

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
      product: info.product,
      mode: info.mode,
      backend: info.backend,
      abi: info.abi,
    },
    null,
    2,
  ),
);

if (info.backend === 'napi' && !process.versions.electron) {
  // Runtime profiles are selected from the public host marker. Unsupported
  // Electron versions must stop before any Electron-specific private ABI is
  // attempted; keep this in a separate final probe so the normal Node path
  // above remains the behavior under test for the rest of this file.
  Object.defineProperty(process.versions, 'electron', {
    configurable: true,
    value: '42.0.0',
  });
  assert.throws(
    () => addon.requireBuiltin('internal/modules/esm/loader'),
    (error) => {
      assert.equal(error.code, 'Unsupported/no-context');
      assert.match(error.message, /unsupported Electron version: 42\.0\.0/);
      return true;
    },
  );
  delete process.versions.electron;
}
