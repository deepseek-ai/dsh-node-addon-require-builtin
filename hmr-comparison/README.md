# HMR cache invalidation comparison

This directory is a standalone verification project for comparing module cache
invalidation strategies.

It does not test whether `node-addon-require-builtin` itself works. The addon
has its own tests for that. This project compares HMR/cache invalidation
strategies from the upstream POC and adds addon-based internal ESM loader access
as another strategy in the same matrix.

## Boundaries

- Do not edit `../src` from here.
- Fixtures and runner code live in this directory so the comparison is
  reproducible inside this repository.
- The addon strategies load the entry package at `../packages/entry`; build the
  parent addon or provide a current-platform optional prebuild before running
  the comparison.

## Install and run

From this directory:

```sh
npm install
npm test
```

For local development without installing here, the parent `node_modules`
can also provide TypeScript:

```sh
../node_modules/.bin/tsc -p tsconfig.json
node dist/runner/index.mjs
```

The runner starts each `scenario x strategy` case in a separate Node process.
The main process does not need `--expose-gc`; each strategy declares its own
`nodeArgs`.

Addon strategies use the package loader's normal optional-prebuild-first
selection. To force a local build during development, run the compiled runner
with `DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE=1`. To verify a prebuild
artifact, use `DSH_NODE_ADDON_INTERNAL_DISABLE_LOCAL_BUILD=1` and set
`DSH_NODE_ADDON_INTERNAL_BACKEND=napi` or `nodeabi`.

## Scenarios

### `esm-static-import`

```js
// fixtures/esm-static-import/a.mjs
import { value, payload } from './b.mjs'
export { value, payload }
```

```js
// fixtures/esm-static-import/b.mjs
export const value = Math.random()
export const payload = new ArrayBuffer(100 * 1024)
```

### `cjs-require`

```js
// fixtures/cjs-require/a.cjs
const b = require('./b.cjs')
module.exports = { value: b.value, payload: b.payload }
```

```js
// fixtures/cjs-require/b.cjs
module.exports = {
  value: Math.random(),
  payload: new ArrayBuffer(100 * 1024),
}
```

The random `value` answers whether module code ran again. The `payload` is
registered with `FinalizationRegistry` so the runner can observe whether the old
module export object became collectable after cache invalidation.

## Strategy matrix

### `naive`

Loads the same entry twice without invalidating anything.

```ts
return loadNative(entry, scenario)
```

Expected baseline: ESM and CJS both reuse cached modules.

### `requirecache`

For CJS only, recursively deletes `require.cache` and removes parent
`module.children` references before the second load.

```ts
if (scenario.format === 'cjs' && loadCount > 0) {
  purgeRequireCache(entry)
}
return loadNative(entry, scenario)
```

Expected: works for `cjs-require`, does not affect ESM.

### `hookversion`

Uses public `module.registerHooks()` to append a version query to fixture file
URLs during resolution.

```ts
hookHandle = registerHooks({
  resolve(specifier, context, nextResolve) {
    const result = nextResolve(specifier, context)
    if (version === 0 || !isFixtureFileUrl(result.url)) return result
    return { ...result, url: `${stripSearchAndHash(result.url)}?v=${version}` }
  },
})
```

Expected: ESM sees new module instances, but old ESM module records are not
fully evicted from Node's module map.

### `hookversion-and-requirecache`

Combines `hookversion` for ESM with `require.cache` eviction for CJS.

Expected: covers both current fixture formats, but ESM old payloads remain held.

### `internalesm`

Uses Node's internal ESM loader through `--expose-internals`:

```ts
const loaderModule = require('internal/modules/esm/loader')
const loadCache = loaderModule.getOrInitializeCascadedLoader().loadCache
```

Before the second ESM load, it deletes fixture entries from `loadCache`.

Expected: true ESM cache eviction for fixture URLs, but this requires
`--expose-internals`.

### `internalesm-and-requirecache`

Combines exposed-internals ESM `loadCache` eviction with CJS `require.cache`
eviction.

Expected: works for both current fixture formats, but still requires
`--expose-internals`.

### `addoninternalesm`

Uses this repository's addon instead of `--expose-internals`:

```ts
const addon = require(entryPackageRoot)
const loaderModule = addon.requireBuiltin('internal/modules/esm/loader')
const loadCache = loaderModule.getOrInitializeCascadedLoader().loadCache
```

Before the second ESM load, it deletes fixture entries from the same internal
ESM `loadCache`.

Expected: same behavior as `internalesm` for ESM, without passing
`--expose-internals`.

### `addoninternalesm-and-requirecache`

Combines addon-based ESM `loadCache` eviction with CJS `require.cache` eviction.

Expected: same behavior as `internalesm-and-requirecache`, without passing
`--expose-internals`.

### `vmsourcetextmodule`

Builds a fresh `vm.SourceTextModule` graph for ESM each time.

Expected: ESM code re-executes, but this is a separate module system and does
not exercise Node's native ESM loader cache. It also demonstrates a practical
retention problem: a fresh graph does not imply that the previous graph is
collectable.

In this harness, `loadEsmGraph()` returns only the entry namespace, and
`loadAndTrack()` keeps only the primitive random `value` plus a
`FinalizationRegistry` registration for `payload`. It does not retain the
namespace, `SourceTextModule`, `ModuleWrap`, or per-load module map after each
iteration returns. When this strategy reports `changed=yes` and `finalized=no`,
the old payload is still reachable through Node/V8 module state rather than
through the harness.

Native ESM also uses `ModuleWrap`. The difference is not "`ModuleWrap` versus
no `ModuleWrap`"; it is the owner and eviction point. The built-in ESM loader
creates `ModuleWrap` through `compileSourceTextModule()` and stores the resulting
module jobs in the loader caches, so deleting matching `loadCache` entries can
remove the loader's strong references. The `vm.SourceTextModule` API creates a
separate graph outside those loader caches, registers callback state for each
module, and has no equivalent public or internal cache entry for this harness to
delete.

Actual Node v24.18.0 observations on this fixture:

```text
strategy                            scenario           changed  finalized
vmsourcetextmodule                  esm-static-import  yes      no
vmsourcetextmodule-noregistermodule esm-static-import  yes      yes
internalesm                         esm-static-import  yes      yes
hookversion                         esm-static-import  yes      no
naive                               esm-static-import  no       -
requirecache                        cjs-require        yes      yes
```

The `vmsourcetextmodule-noregistermodule` strategy is an isolation check that
points to Node's `registerModule()` path as the root edge. With the same
`vm.SourceTextModule` graph, the default behavior leaves the payload
unfinalized. If `--expose-internals` is used to set
`require('internal/modules/esm/utils').registerModule = () => {}` before
constructing the modules, the payload finalizes. That rules out a standalone V8
module-record cycle as the retaining root for this fixture.

- `lib/internal/modules/esm/utils.js:337-355`: native ESM also compiles source
  text into a `ModuleWrap`; this is why `ModuleWrap` itself is not the
  distinguishing factor.
- `lib/internal/vm/module.js:312-323`: every `vm.SourceTextModule` registers its
  `ModuleWrap` with `internal/modules/esm/utils.registerModule()`, and the
  registry stores `callbackReferrer = this`, i.e. the JS `SourceTextModule`
  wrapper itself.
- `lib/internal/modules/esm/utils.js:151-184`: Node documents the retention
  chain for host-defined options. V8 keeps the host-defined-options symbol alive
  while `import()` can still be initiated; that symbol keeps the WeakMap value
  alive; the value keeps `callbackReferrer` alive.
- `src/module_wrap.cc:727-729`: `ModuleWrap::Link()` stores the linked
  dependency array in the `kLinkedRequestsSlot` internal field.
- `src/module_wrap.h:101-102` and `src/module_wrap.h:219-223`: Node documents
  that `kLinkedRequestsSlot` contains linked `ModuleWrap` JS wrapper objects and
  that this array is the actual strong reference keeping linked modules alive.
- `src/module_wrap.h:126-129`: Node explicitly marks `ModuleWrap` objects as
  not indicative of leaks at exit, with the comment: "The garbage collection
  rules for ModuleWrap are *super* unclear. Do these objects ever get GC'd? Are
  we just okay with leaking them?"
- `deps/v8/src/objects/source-text-module.tq:12-26`: V8
  `SourceTextModule` stores `regular_exports`, `regular_imports`, and
  `requested_modules`.
- `deps/v8/src/objects/source-text-module.cc:135-149` and
  `deps/v8/src/objects/source-text-module.cc:173-179`: V8 creates export
  `Cell`s, stores them in the module's export tables, and writes evaluated
  export values into those cells.
- `deps/v8/src/objects/module.tq:6-23` and
  `deps/v8/src/objects/module.cc:320-387`: V8 stores a namespace cell on the
  module, and the `JSModuleNamespace` stores a back-pointer to the module.

The retaining root path observed here is:

```text
v8::Module/Script host-defined options
  -> idSymbol
  -> internal/modules/esm/utils moduleRegistries value
  -> callbackReferrer (vm.SourceTextModule)
  -> kWrap (ModuleWrap)
  -> kLinkedRequestsSlot child ModuleWrap array
  -> evaluated module records / namespace exports
  -> fixture payload
```

If the WeakMap/registry edge and `ModuleWrap` edge are excluded, the remaining
V8 edges are only internal graph edges:

```text
SourceTextModule
  -> requested_modules / regular_exports / exports
  -> export Cell
  -> fixture payload

SourceTextModule
  -> module namespace Cell
  -> JSModuleNamespace
  -> SourceTextModule
```

Those edges explain how a live module reaches the payload, but they do not by
themselves explain why the module is live. In the no-op `registerModule()`
experiment, this V8 graph is collected as a unit.

### `vmsourcetextmodule-noregistermodule`

Validation-only control for `vmsourcetextmodule`. It uses the same
`vm.SourceTextModule` graph builder, but runs with `--expose-internals` and
patches Node's JS internal ESM utilities before constructing any modules:

```ts
const utils = require('internal/modules/esm/utils')
utils.registerModule = () => {}
```

Expected: ESM code re-executes and the old payload finalizes. This is not a
usable HMR strategy because it disables Node's callback registry for
`vm.SourceTextModule`; it exists to isolate whether the default retention comes
from the `registerModule()` host-defined-options path or from a V8 module graph
cycle alone.

### `isolatedvm`

Uses the optional `isolated-vm` package to create an entirely separate V8
isolate and load a fresh ESM/CJS graph inside it.

```ts
const isolate = new ivm.Isolate({ memoryLimit: 8 })
const context = isolate.createContextSync()
const module = isolate.compileModuleSync(source, { filename })
```

This strategy is included in source form for parity with the upstream POC, but
the default runner only enables it when `isolated-vm` is installed in this
project.

### `vmcontext`

Runs dynamic `import()` inside a new `vm.Context` while delegating to the main
context default loader.

Expected: still shares the main loader cache; not a true invalidation strategy.

## Output

The runner prints:

```text
strategy                              scenario           changed  finalized  supported
addoninternalesm-and-requirecache     esm-static-import  yes      yes        yes
```

- `changed=yes`: second load saw a different random value.
- `finalized=yes`: the stale payload from the old execution was collected.
- `finalized=-`: no stale payload existed because no reload happened.
- `supported=yes`: this strategy caused a behavior change for that scenario.

## Why this copy exists

The upstream POC compares public hooks, CJS cache deletion, exposed internal ESM
loader access, and VM-based alternatives. This repository adds another internal
loader access path: `node-addon-require-builtin` can obtain
`internal/modules/esm/loader` without `--expose-internals`.

Keeping the comparison here makes the addon strategy reviewable next to the
addon package, while preserving the upstream POC as read-only source material.
