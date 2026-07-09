'use strict';

// Cross-platform correctness contract: the native addon must resolve the *same*
// module objects that Node exposes under --expose-internals. darwin, linux, and
// win32 all run this and must produce identical require results.
//
// Run with --expose-internals so the genuine internal modules are requirable
// for comparison. The addon itself never needs that flag; it probes private
// runtime state directly, and this test proves that probe lands on exactly the
// object Node would hand out.
const assert = require('node:assert/strict');
const { entryPackagePath } = require('./product.js');

assert.ok(
  process.execArgv.includes('--expose-internals'),
  'run this test with --expose-internals so the genuine internals are comparable',
);

const addon = require(entryPackagePath());

const cases = [
  'internal/modules/esm/loader',
  'internal/modules/cjs/loader',
];

for (const id of cases) {
  const genuine = require(id);
  assert.equal(addon.isAllowedInternalId(id), true);
  const viaAddon = addon.requireBuiltin(id);
  assert.equal(
    typeof viaAddon,
    'object',
    `${id}: addon did not return a module object`,
  );
  assert.strictEqual(
    viaAddon,
    genuine,
    `${id}: addon result must be identical to the genuine internal module`,
  );
  console.log(`require-parity ok: ${id}`);
}

const info = addon.getBindingInfo();
console.log(
  JSON.stringify(
    {
      platform: process.platform,
      arch: process.arch,
      node: process.version,
      backend: info.backend,
      abi: info.abi,
      binding_source: info.bindingSource,
    },
    null,
    2,
  ),
);
