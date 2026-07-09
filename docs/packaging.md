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
```

When changing the matrix, update package metadata, all matching
`prebuilds.json` files, the lockfile, and support/release docs in the same
change.

Project-wide naming conventions are documented in [naming.md](naming.md).

## Runtime Selection

The main package loader:

1. Computes the platform suffix.
2. Loads `@esplus/node-addon-require-builtin-<platform>` when installed.
3. Reads the platform package `prebuilds.json`.
4. Loads the `napi-v9` binary in `auto` mode.
5. Fails closed when no optional prebuild works. Published packages do not
   compile native sources at install time.

Every native binding must return matching `backend` and `abi` values from
`getNativeBindingInfo()`. Loading a binary with the wrong metadata is an error
even if `dlopen` succeeds.

The loader materializes the selected `.node` file into a runtime cache before
loading it. This avoids locking package-managed files under `node_modules` on
platforms such as Windows. Set `NARB_NATIVE_CACHE_DIR` to override the cache
location, or `NARB_DISABLE_NATIVE_CACHE=1` for debugging. The cache path is
`<root>/<package>/<version>/<platform>/<file>` — namespaced by package name,
package version, platform and original file name, with no content hash in the
path so it stays short on Windows. The content hash is used only to verify the
cached file; if it does not match, or cache creation fails, the loader falls
back to the original package-managed path.

## Install Fallback

The main package includes `scripts/install.js`. If no optional prebuild passes
validation, installation fails with a clear unsupported message. Source builds
are repository development and CI workflows only; private runtime packages do
not publish native sources or run `node-gyp` at install time.

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

Use `pnpm test:nodeabi` after preparing headers to validate the source-build
nodeabi backend for the current runtime. Nodeabi binaries are not published.

Generated `.node` files are ignored by git. Release automation should build
them on the matching platform before publishing. Normal CI artifacts are
verification outputs only; release publishing rebuilds the full `prebuilds.json`
matrix from the release tag and publishes the assembled tarballs.
