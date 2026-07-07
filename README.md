# @deepseek-ai/dsh-node-addon-internal

Node-API addon that obtains Node's internal `requireBuiltin()`
without starting Node with `--expose-internals`.

This is not a pure public Node-API implementation. The addon is loaded through a
stable N-API v9 entry point, but the useful work probes private Node/V8 runtime
state and validates the result at runtime. If a runtime does not match the
expected invariants, the addon must fail closed instead of guessing offsets.

## Install

```sh
npm install @deepseek-ai/dsh-node-addon-internal
```

Published packages use a main package plus platform optional packages:

```text
@deepseek-ai/dsh-node-addon-internal
@deepseek-ai/dsh-node-addon-internal-darwin-arm64
@deepseek-ai/dsh-node-addon-internal-darwin-x64
@deepseek-ai/dsh-node-addon-internal-linux-arm64-gnu
@deepseek-ai/dsh-node-addon-internal-linux-x64-gnu
@deepseek-ai/dsh-node-addon-internal-win32-arm64-msvc
@deepseek-ai/dsh-node-addon-internal-win32-ia32-msvc
@deepseek-ai/dsh-node-addon-internal-win32-x64-msvc
@deepseek-ai/dsh-node-addon-internal-loader
```

The main package first tries the current platform optional package, then falls
back to a local `node-gyp` build under `packages/entry/build/`.

## Usage

```js
const internalAddon = require('@deepseek-ai/dsh-node-addon-internal');

const esmLoader = internalAddon.getModulesEsmLoader();
const cascadedLoader = esmLoader.getOrInitializeCascadedLoader();
```

The public API is intentionally small:

- `getModulesCjsLoader()`: returns `internal/modules/cjs/loader`.
- `getModulesEsmLoader()`: returns `internal/modules/esm/loader`.
- `getBindingInfo()`: lazily returns binding diagnostics such as `mode`,
  `backend`, `abi`, `bindingSource`, and `bindingPath`.

Treat returned internal modules as unstable Node implementation details. This
package does not make Node internals public API.

## Backends

The native implementation has two backend dimensions:

- `napi`: default release backend. It builds one `napi-v9` binary per supported
  platform/arch and discovers private Node/V8 state at runtime.
- `nodeabi`: Node-major/ABI backend. It is selected at compile time and uses
  matching official Node.js public headers. It still avoids Node source private
  headers.

The loader defaults to `auto`: it tries a matching `nodeabi` binary when the
platform package contains one for `process.versions.modules`, then falls back to
`napi-v9`. Set `DSH_NODE_ADDON_INTERNAL_BACKEND=napi` or
`DSH_NODE_ADDON_INTERNAL_BACKEND=nodeabi` to force one backend.

## Development

```sh
corepack enable
pnpm install
pnpm build
pnpm test
```

`pnpm build` builds TypeScript entrypoints and the N-API native addon into
`packages/entry/build/`. It does not need separately downloaded public headers
beyond the current runtime headers used by `node-gyp`/Node.

For backend comparison with official Node.js public headers:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

For current-platform prebuild preparation:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm build:prebuild:napi
pnpm build:prebuild:nodeabi
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

Supported optional packages publish `napi-v9` plus Node-major-specific
`nodeabi` binaries for Node 20, 22, 24, and 26. Linux musl is not published yet.
Node.js stopped shipping 32-bit Windows binaries after v22, so `win32-ia32-msvc`
covers only Node 20 and 22.
See [docs/support-matrix.md](docs/support-matrix.md).

## License

MIT
