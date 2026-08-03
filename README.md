# Node addon internal require products

Node-API addons that obtain Node's internal `requireBuiltin()` without starting
Node with `--expose-internals`.

These are not pure public Node-API implementations. Each addon is loaded
through a stable N-API v9 entry point, but the useful work probes private
Node/V8 runtime state and validates the result at runtime. If a runtime does
not match the expected invariants, the addon must fail closed instead of
guessing offsets.

## Install

```sh
npm install node-addon-internal-loader
# or
npm install node-addon-require-builtin
```

Published packages use two product families plus one shared loader package:

```text
node-addon-native-custom-loader
node-addon-require-builtin
node-addon-require-builtin-darwin-arm64
node-addon-require-builtin-darwin-x64
node-addon-require-builtin-linux-arm64-gnu
node-addon-require-builtin-linux-x64-gnu
node-addon-require-builtin-win32-arm64-msvc
node-addon-require-builtin-win32-ia32-msvc
node-addon-require-builtin-win32-x64-msvc
node-addon-internal-loader
node-addon-internal-loader-darwin-arm64
node-addon-internal-loader-darwin-x64
node-addon-internal-loader-linux-arm64-gnu
node-addon-internal-loader-linux-x64-gnu
node-addon-internal-loader-win32-arm64-msvc
node-addon-internal-loader-win32-ia32-msvc
node-addon-internal-loader-win32-x64-msvc
```

Each entry package requires a CI-validated current-platform optional package.
If no compatible optional prebuild is available, installation fails closed
instead of compiling an unvalidated local binary.

## Usage

Use `node-addon-internal-loader` when only the CommonJS and ESM loader
internals are needed:

```js
const internalLoader = require('node-addon-internal-loader');

const esmLoader = internalLoader.requireBuiltin('internal/modules/esm/loader');
const cascadedLoader = esmLoader.getOrInitializeCascadedLoader();
```

Use `node-addon-require-builtin` when unrestricted internal builtin
loading is required:

```js
const requireBuiltinAddon = require('node-addon-require-builtin');

const realm = requireBuiltinAddon.requireBuiltin('internal/bootstrap/realm');
```

Both entry packages expose the same small API:

- `requireBuiltin(moduleId)`: returns the selected internal module.
- `isAllowedInternalId(moduleId)`: returns whether `moduleId` is loadable.
- `getBindingInfo()`: lazily returns binding diagnostics such as `mode`,
  `product`, `backend`, `abi`, `bindingSource`, and `bindingPath`.

`node-addon-internal-loader` only allows
`internal/modules/cjs/loader` and `internal/modules/esm/loader`; the allowlist
is enforced in the native addon before calling Node's internal
`requireBuiltin()`. `node-addon-require-builtin` does not restrict
module ids: `isAllowedInternalId()` always returns `true`, and
`requireBuiltin(id)` forwards any string id to Node.
See [docs/internal-modules.md](docs/internal-modules.md) for the allowlist and
supported Node version ranges.

Treat returned internal modules as unstable Node implementation details. These
packages do not make Node internals public API.

## Backends

The native implementation has one product dimension and one backend dimension:

Products:

- `internal-loader`: default product. It enforces the CJS/ESM loader allowlist.
- `require-builtin`: unrestricted product. It forwards any string id to Node's
  builtin require.

Backends:

- `napi`: default release backend. It builds one `napi-v9` binary per supported
  platform/arch and discovers private Node/V8 state at runtime.
- `nodeabi`: Node-major/ABI backend used for source-build validation in this
  repository. It is not published as an optional prebuild artifact.

The published loader defaults to `auto`, which resolves to the current
platform's `napi-v9` optional prebuild. Set `NARB_BACKEND=napi` to force that
backend. Source builds and product-specific tests use
`NARB_PRODUCT=internal-loader` or `NARB_PRODUCT=require-builtin`.

## Development

```sh
corepack enable
pnpm install
pnpm build
pnpm test
```

`pnpm build` builds TypeScript entrypoints and the N-API native addon into
`packages/internal-loader/entry/build/` by default. Set
`NARB_PRODUCT=require-builtin` to build the unrestricted product into
`packages/require-builtin/entry/build/`. Source builds are a repository
development and CI workflow only; published packages do not include native
sources for install-time fallback builds.

For backend comparison with official Node.js public headers:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

For current-platform prebuild preparation:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm build:prebuild:napi
pnpm test:optional
```

See [docs/development.md](docs/development.md) for the full local workflow,
[docs/architecture.md](docs/architecture.md) for the runtime design, and
[docs/packaging.md](docs/packaging.md) for the optional package model.

## Support Status

The supported optional prebuild target set is intentionally conservative:

| Platform | Node 20 | Node 22 | Node 24 | Node 26 |
|---|---|---|---|---|
| macOS arm64 (`darwin-arm64`) | Supported | Supported | Supported | Supported |
| macOS x64 (`darwin-x64`) | Supported | Supported | Supported | Supported |
| Linux glibc arm64 (`linux-arm64-gnu`) | Supported | Supported | Supported | Supported |
| Linux glibc x64 (`linux-x64-gnu`) | Supported | Supported | Supported | Supported |
| Windows arm64 MSVC (`win32-arm64-msvc`) | Supported | Supported | Supported | Supported |
| Windows x86 MSVC (`win32-ia32-msvc`) | Supported | Supported | No 32-bit runtime | No 32-bit runtime |
| Windows x64 MSVC (`win32-x64-msvc`) | Supported | Supported | Supported | Supported |

Both product families publish one `napi-v9` binary per supported platform,
tested across Node 20, 22, 24, and 26. Linux GNU prebuilds require glibc 2.28 or
newer and a `GLIBCXX_3.4.25`-compatible C++ runtime. Linux musl is not published yet.
Node.js stopped shipping 32-bit Windows binaries after v22, so
`win32-ia32-msvc` covers only Node 20 and 22.
See [docs/support-matrix.md](docs/support-matrix.md) and
[docs/internal-modules.md](docs/internal-modules.md).

## License

MIT
