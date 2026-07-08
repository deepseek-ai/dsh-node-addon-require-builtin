'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');

const {
  bundledNodeGypScript,
  nodeGypArgsFor,
  nodeGypDefines,
  nodeGypInvocationFor,
  sourceFallbackAvailable,
  validationScript,
} = require('../packages/entry/scripts/install.js');

assert.deepEqual(
  nodeGypInvocationFor('linux', undefined, '/usr/bin/node'),
  {
    command: 'node-gyp',
    args: ['rebuild'],
    shell: false,
  },
);

assert.deepEqual(
  nodeGypInvocationFor('win32', undefined, 'C:\\node\\node.exe', undefined, () => false),
  {
    command: 'node-gyp.cmd',
    args: ['rebuild'],
    shell: true,
  },
);

{
  const entryNodeGyp = path.resolve(
    'packages/entry/node_modules/node-gyp/bin/node-gyp.js',
  );
  const exists = (candidate) => candidate === entryNodeGyp;
  assert.equal(
    bundledNodeGypScript('/node/node', undefined, exists),
    entryNodeGyp,
  );
}

assert.deepEqual(
  nodeGypInvocationFor(
    'win32',
    'C:\\Program Files\\node-gyp\\node-gyp.cmd',
    'C:\\node\\node.exe',
  ),
  {
    command: 'C:\\Program Files\\node-gyp\\node-gyp.cmd',
    args: ['rebuild'],
    shell: true,
  },
);

assert.deepEqual(
  nodeGypInvocationFor(
    'win32',
    'C:\\pnpm\\node_modules\\node-gyp\\bin\\node-gyp.js',
    'C:\\node\\node.exe',
  ),
  {
    command: 'C:\\node\\node.exe',
    args: ['C:\\pnpm\\node_modules\\node-gyp\\bin\\node-gyp.js', 'rebuild'],
    shell: false,
  },
);

{
  const pnpmActionNodeGyp = path.resolve(
    '/setup-pnpm/node_modules/pnpm/dist/node_modules/node-gyp/bin/node-gyp.js',
  );
  const exists = (candidate) => candidate === pnpmActionNodeGyp;
  assert.equal(
    bundledNodeGypScript(
      '/node/node',
      '/setup-pnpm/node_modules/pnpm/bin/pnpm.cjs',
      exists,
    ),
    pnpmActionNodeGyp,
  );
  assert.deepEqual(
    nodeGypInvocationFor(
      'win32',
      undefined,
      '/node/node',
      '/setup-pnpm/node_modules/pnpm/bin/pnpm.cjs',
      exists,
    ),
    {
      command: '/node/node',
      args: [pnpmActionNodeGyp, 'rebuild'],
      shell: false,
    },
  );
}

{
  const pnpmShimNodeGyp = path.resolve(
    '/setup-pnpm/node_modules/pnpm/dist/node_modules/node-gyp/bin/node-gyp.js',
  );
  const exists = (candidate) => candidate === pnpmShimNodeGyp;
  assert.equal(
    bundledNodeGypScript(
      '/node/node',
      '/setup-pnpm/node_modules/.bin/pnpm.CMD',
      exists,
    ),
    pnpmShimNodeGyp,
  );
}

{
  const corepackNodeGyp = path.resolve(
    '/corepack/v1/pnpm/9.15.9/dist/node_modules/node-gyp/bin/node-gyp.js',
  );
  const exists = (candidate) => candidate === corepackNodeGyp;
  assert.equal(
    bundledNodeGypScript(
      '/node/node',
      '/corepack/v1/pnpm/9.15.9/dist/pnpm.cjs',
      exists,
    ),
    corepackNodeGyp,
  );
}

{
  const npmNodeGyp = path.resolve(
    '/node/node_modules/npm/node_modules/node-gyp/bin/node-gyp.js',
  );
  const exists = (candidate) => candidate === npmNodeGyp;
  assert.equal(
    bundledNodeGypScript(
      '/node/node',
      undefined,
      exists,
    ),
    npmNodeGyp,
  );
}

assert.equal(
  nodeGypDefines(undefined),
  'enable_lto=false enable_thin_lto=false lto_jobs=',
);

assert.equal(
  nodeGypDefines('target_arch=x64'),
  'target_arch=x64 enable_lto=false enable_thin_lto=false lto_jobs=',
);

assert.deepEqual(
  nodeGypArgsFor('linux', ['rebuild']),
  ['rebuild'],
);

assert.deepEqual(
  nodeGypArgsFor('win32', ['rebuild']),
  [
    'rebuild',
    '--enable-lto=false',
    '--enable-thin-lto=false',
    '--lto-jobs=',
  ],
);

assert.deepEqual(
  nodeGypArgsFor('win32', ['C:\\node-gyp\\bin\\node-gyp.js', 'rebuild']),
  [
    'C:\\node-gyp\\bin\\node-gyp.js',
    'rebuild',
    '--enable-lto=false',
    '--enable-thin-lto=false',
    '--lto-jobs=',
  ],
);

assert.equal(
  sourceFallbackAvailable(),
  true,
  'source fallback should be available from a repository checkout',
);

{
  const script = validationScript();
  assert.match(script, /error\.diagnostics/);
  assert.match(script, /error\.stack/);
  assert.match(script, /code:/);
  assert.match(script, /keys=/);
  assert.match(script, /validateInternalModule\('internal\/modules\/esm\/loader'/);
  assert.match(script, /validateInternalModule\('internal\/modules\/cjs\/loader'/);
  assert.ok(
    script.indexOf("validateInternalModule('internal/modules/esm/loader'") <
      script.indexOf("validateInternalModule('internal/modules/cjs/loader'"),
    'install validation should probe ESM before CJS',
  );
}

{
  const installScript = require('node:fs').readFileSync(
    path.resolve('packages/entry/scripts/install.js'),
    'utf8',
  );
  assert.match(installScript, /sourceFallbackAvailable/);
  assert.match(installScript, /writeValidationOutput\(result\)/);
  assert.match(installScript, /local build validation: ok/);
  assert.match(installScript, /Source fallback is disabled/);
}
