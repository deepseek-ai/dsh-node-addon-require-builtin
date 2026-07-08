# Packaging

The package uses the same broad shape as native packages such as esbuild and
Rollup: one JS entry package plus platform optional packages.

## Published Packages

```text
@esplus/node-addon-require-builtin
@esplus/node-addon-require-builtin-loader
@esplus/node-addon-require-builtin-darwin-arm64
@esplus/node-addon-require-builtin-darwin-x64
@esplus/node-addon-require-builtin-linux-arm64-gnu
@esplus/node-addon-require-builtin-linux-x64-gnu
@esplus/node-addon-require-builtin-win32-arm64-msvc
@esplus/node-addon-require-builtin-win32-ia32-msvc
@esplus/node-addon-require-builtin-win32-x64-msvc
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
2. Loads `@esplus/node-addon-require-builtin-<platform>` when installed.
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
local build layout, and validates both default allowlisted modules.

Install validation calls `requireBuiltin('internal/modules/cjs/loader')` and
`requireBuiltin('internal/modules/esm/loader')` through the package API because
a binary that loads but cannot obtain those internal modules is not usable for
this package.

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
them on the matching platform before publishing. Normal CI artifacts are
verification outputs only; release publishing rebuilds the full `prebuilds.json`
matrix from the release tag and publishes the assembled tarballs.

## Naming Conventions

The public npm package family uses the `@esplus` scope and the
`node-addon-require-builtin` package prefix. Keep package names aligned with the
runtime loader prefix:

```text
@esplus/node-addon-require-builtin
@esplus/node-addon-require-builtin-loader
@esplus/node-addon-require-builtin-<platform>
```

Runtime environment variables use the short project prefix `NARB_`, standing for
Node Addon Require Builtin:

```text
NARB_BACKEND
NARB_BUILD_OUTPUT
NARB_DISABLE_OPTIONAL_PACKAGE
NARB_DISABLE_LOCAL_BUILD
NARB_EXPECTED_BACKEND
NARB_HEADERS_CACHE
NARB_TRACE
```

C/C++ preprocessor macros use the same `NARB_` prefix:

```cpp
NARB_BACKEND
NARB_BACKEND_NAPI
NARB_BACKEND_NODEABI
NARB_BACKEND_NODEABI_VERIFY
NARB_MEMBER_ABI
NARB_PRINTF_FORMAT
```

C++ headers should use `#pragma once` instead of include guards.

C++ implementation symbols live under the organization and feature namespace:

```cpp
namespace esplus::node::require_builtin {
}
```

The native addon binary and node-gyp target are named after the public capability,
not the probing implementation:

```text
require_builtin.node
NODE_API_MODULE(require_builtin, Init)
```

Probe source files use `require_builtin_probe.*`. Avoid the old
`internal_require_probe.*` naming in new code.
