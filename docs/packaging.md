# Packaging

The package uses the same broad shape as native packages such as esbuild and
Rollup: one JS entry package plus platform optional packages.

## Published Packages

```text
@deepseek-ai/dsh-node-addon-internal
@deepseek-ai/dsh-node-addon-internal-loader
@deepseek-ai/dsh-node-addon-internal-darwin-arm64
@deepseek-ai/dsh-node-addon-internal-darwin-x64
@deepseek-ai/dsh-node-addon-internal-linux-arm64-gnu
@deepseek-ai/dsh-node-addon-internal-linux-x64-gnu
@deepseek-ai/dsh-node-addon-internal-win32-arm64-msvc
@deepseek-ai/dsh-node-addon-internal-win32-ia32-msvc
@deepseek-ai/dsh-node-addon-internal-win32-x64-msvc
```

Unsupported platforms are intentionally absent from `optionalDependencies`.

## Package Matrix

The package matrix is explicit in checked-in package metadata:

- `packages/entry/package.json` lists published optional dependencies.
- `packages/<platform>/package.json` declares `os`, `cpu`, and Linux `libc`.
- `packages/<platform>/prebuilds.json` declares the backend/ABI binaries that
  may exist in that platform package.
- `docs/support-matrix.md` explains why unsupported platform packages are not
  published.

`scripts/build.ts` derives only the current runtime suffix. In prebuild mode it
writes to an existing `packages/<platform>/prebuilt/` directory and fails if the
platform package does not exist. It is not a package-matrix generator.

Platform package names contain only platform information. Backend and ABI are
encoded in filenames inside the platform package:

```text
prebuilt/<platform>-napi-v9.node
prebuilt/<platform>-nodeabi-v115.node
prebuilt/<platform>-nodeabi-v127.node
prebuilt/<platform>-nodeabi-v137.node
prebuilt/<platform>-nodeabi-v147.node
```

When changing the matrix, update package metadata, all matching
`prebuilds.json` files, the lockfile, and support/release docs in the same
change.

## Runtime Selection

The main package loader:

1. Computes the platform suffix.
2. Loads `@deepseek-ai/dsh-node-addon-internal-<platform>` when installed.
3. Reads the platform package `prebuilds.json`.
4. Tries a matching `nodeabi` binary first in `auto` mode.
5. Falls back to `napi-v9`.
6. Falls back to local build output if allowed.

Every native binding must return matching `backend` and `abi` values from
`getNativeBindingInfo()`. Loading a binary with the wrong metadata is an error
even if `dlopen` succeeds.

## Install Fallback

The main package includes `scripts/install.js`. If no optional prebuild passes
validation, it runs `node-gyp rebuild`, copies the resulting N-API binary to the
local build layout, and validates both loader getters.

Install validation calls `getModulesCjsLoader()` and `getModulesEsmLoader()`
because a binary that loads but cannot obtain those internal modules is not
usable for this package.

## Release Outputs

Local release preparation for the current platform:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm build:prebuilds
pnpm test:optional
```

Use `pnpm test:optional:nodeabi` after preparing headers to validate the
Node-major-specific optional prebuild for the current runtime.

Generated `.node` files are ignored by git. Release automation should build
them on the matching platform before publishing.
