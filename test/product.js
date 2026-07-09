'use strict';

// Test-side product/family selection. Two published families build from one
// source tree; NARB_PRODUCT (or NARB_EXPECTED_PRODUCT) picks which family's
// entry package the tests exercise. Defaults to internal-loader — the
// whitelisted variant — matching the native NARB_PRODUCT default.
const path = require('node:path');

function toPortablePath(value) {
  return value.replace(/\\/g, '/');
}

const PRODUCTS = {
  'require-builtin': {
    product: 'require-builtin',
    family: 'require-builtin',
    entryPackageName: '@esplus/node-addon-require-builtin',
    enforcesWhitelist: false,
  },
  'internal-loader': {
    product: 'internal-loader',
    family: 'internal-loader',
    entryPackageName: '@esplus/node-addon-internal-loader',
    enforcesWhitelist: true,
  },
};

function productConfig() {
  const requested = process.env.NARB_EXPECTED_PRODUCT ||
    process.env.NARB_PRODUCT ||
    'internal-loader';
  const config = PRODUCTS[requested];
  if (!config) {
    throw new Error(`unknown NARB_PRODUCT: ${requested}`);
  }
  return config;
}

function entryDir() {
  return path.resolve(__dirname, '..', 'packages', productConfig().family, 'entry');
}

function entryPackagePath() {
  return entryDir();
}

module.exports = {
  toPortablePath,
  productConfig,
  entryDir,
  entryPackagePath,
};
