# Development

## Prerequisites

- Node.js 20 or newer.
- Corepack-enabled pnpm.
- A C++ compiler supported by Node native addons.
- `curl` for `nodeabi` header downloads.

## First Setup

```sh
corepack enable
pnpm install
pnpm build
pnpm test
```

`pnpm build` compiles TypeScript package entrypoints and the default N-API native
addon. `pnpm test` runs the addon without `--expose-internals`.

## Common Commands

```sh
pnpm build
pnpm build:ts
pnpm build:native:napi
pnpm test
pnpm test:optional
pnpm typecheck
```

Backend comparison needs official Node.js public headers for the current runtime:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

The header helper downloads official archives from Node's release server and
prints `NODE_JS_PUBLIC_INCLUDE_DIRS=...` for the current shell.

## Multi-runtime Testing

```sh
pnpm test:all
```

This command expects Node 20, 22, 24, and 26 installations under nvm. You can
override discovery with `NODE_TEST_BINS`, separated by the platform path
delimiter.

## Environment Variables

See [naming.md](naming.md) for the project-wide naming convention.

- `NARB_BACKEND=auto|napi|nodeabi`: backend preference.
- `NARB_DISABLE_OPTIONAL_PACKAGE=1`: skip optional packages.
- `NARB_DISABLE_LOCAL_BUILD=1`: skip local build lookup in loader tests.
- `NARB_EXPECTED_BACKEND=napi|nodeabi`: test assertion helper.
- `NARB_DISABLE_NATIVE_CACHE=1`: load native binaries from their package/local
  build path instead of the runtime native cache.
- `NARB_NATIVE_CACHE_DIR=...`: override the runtime native cache directory.
- `NODE_JS_PUBLIC_INCLUDE_DIRS=...`: official Node.js public header include path
  for `nodeabi` builds.
- `NARB_HEADERS_CACHE=...`: header helper cache directory.
- `NODE_DIST_BASE_URL=...`: alternate Node release mirror.

## Generated Files

Do not commit generated outputs:

```text
.cache/
build/
dist/
packages/*/build/
packages/*/lib/
packages/*/prebuilt/
*.node
*.tsbuildinfo
```
