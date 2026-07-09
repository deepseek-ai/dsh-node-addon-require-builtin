# Architecture

This repository publishes two Node native addon product families that obtain the
bootstrap `requireBuiltin()` function from the current Node `Realm` and expose a
small JS API around it:

- `node-addon-internal-loader`: whitelisted access to the CJS and ESM
  loader internals.
- `node-addon-require-builtin`: unrestricted forwarding to Node's
  builtin require.

Both products have a stable public loading shell, but their core behavior is
private runtime probing:

```text
JS entry package / shared optional package loader
  -> Node-API addon entry
  -> product policy + requireBuiltin decision flow
  -> backend-specific runtime context adapter
       -> napi backend
       -> nodeabi backend
  -> shared private runtime probe
       -> Realm pointer validation
       -> PrincipalRealm::builtin_module_require symbol lookup
       -> platform getter machine-code parser
       -> handle validation through public N-API
  -> smoke tests and target exports load checks
```

## JS Loader Layer

Each published entry package loads one native binary through
`node-addon-native-custom-loader`. The entry packages call
`createEntryApi(packageDir)`, so the shared loader derives product and package
names from the installed entry package instead of hardcoding one family.

Selection order:

1. Resolve the entry package name and product family.
2. Resolve the current platform suffix, such as `darwin-arm64` or
   `linux-x64-gnu`.
3. Try the matching platform optional package unless
   `NARB_DISABLE_OPTIONAL_PACKAGE=1`.
4. In the optional package, load the `napi-v9` binary for the current platform.
5. If no optional package binary works, fail closed. Published packages do not
   compile native sources at install time.

`NARB_BACKEND=napi|nodeabi|auto` controls backend preference in development.
`auto` is the default. `NARB_PRODUCT=internal-loader|require-builtin` selects
the native product for source builds; the default is `internal-loader`.

## Native API Layer

`packages/native/src/node_api_addon.cc` exports:

- `requireBuiltin(moduleId)`
- `isAllowedInternalId(moduleId)`
- `getNativeBindingInfo()`

Each JS entry package re-exports only `requireBuiltin(moduleId)`,
`isAllowedInternalId(moduleId)`, and a lazy `getBindingInfo()` wrapper around
native and loader metadata. It does not expose `probe()` directly.

Before a selected native binary is required, the shared loader copies it to a
runtime cache and loads that copy. This keeps package-managed `.node` files
replaceable by package managers while the process is running. The cache path is
deterministic — namespaced by package name, version and platform — so it stays
short (well under the Windows `MAX_PATH` limit) and identical on every load; a
content hash is used only to verify the cached file's integrity, and if it does
not match (or materialization fails) the loader falls back to the original
binary path.

## Probe Decision Flow

`packages/native/src/require_builtin_probe.cc` owns the backend-independent
control flow:

1. Ask the selected runtime adapter for a candidate `requireBuiltin` value.
2. Verify the JS function name is `requireBuiltin`.
3. Smoke test `requireBuiltin('internal/bootstrap/realm')`.
4. Verify the realm export self-reference points back to the same function.
5. Apply product policy to the requested module id.
6. Load the selected target internal module and record whether it returned
   exports. The getter does not inspect target export properties.

Target-load exceptions are converted into clear unsupported errors with
diagnostics. For `internal-loader`, the CJS/ESM loader allowlist is enforced in
C++ before the private `requireBuiltin()` value is called. For
`require-builtin`, `isAllowedInternalId()` always returns `true` and any string
id is forwarded to Node. See [internal-modules.md](internal-modules.md) for the
whitelisted module ids and Node version ranges.

## Runtime Backends

Both backends implement:

```cpp
Result<RuntimeRequireBuiltin> ProbeRuntimeRequireBuiltin(napi_env env);
```

### `napi`

The `napi` backend builds one N-API v9 binary per supported platform.

Constraints:

- Does not include `node.h` or `v8.h`.
- Does not bind to Node/V8 C++ headers at compile time.
- Finds required V8 symbols dynamically.
- Uses N-API immediately to validate the candidate handle.

### `nodeabi`

The `nodeabi` backend builds one binary per Node module ABI for source-build
validation. It is not published as an optional prebuild artifact.

Constraints:

- Uses official Node.js public headers from `node-vX.Y.Z-headers.tar.gz`.
- May include public `node.h`, `v8.h`, and `node_version.h`.
- Does not use `NODE_WANT_INTERNALS`.
- Does not include Node source private headers.
- Does not declare or include `node::Realm` or `node::PrincipalRealm`.

## Shared Private Probe

`packages/native/src/runtime_probe/helper.cc` receives an opaque `Realm*` from
the backend and performs the private checks:

- Resolve `node::PrincipalRealm::builtin_module_require() const` dynamically.
- On Windows, fall back to scanning the live `PrincipalRealm` vtable for the
  matching getter when the private getter is not exported from the PE image.
  The getter block is identified structurally (longest strictly-ascending field
  offset chain plus exact `requireBuiltin` N-API identity) rather than by a fixed
  slot/offset stride, which is not stable across Windows architectures.
- Verify the getter and `Realm` vtable are from the same loaded image on
  platforms where image metadata is available.
- Parse a short getter machine-code pattern to get the runtime field offset.
- Read the field at `Realm + offset`.
- Call the getter with the platform's C++ member calling convention and require
  it to match the field read. MSVC returns a non-trivial `v8::Local<T>` through a
  hidden struct-return pointer (x64 `rdx`, arm64 `x1`, x86 a `__thiscall` stack
  slot), so the call mode is selected from the decoded getter shape.
- Validate the resulting handle is a function via public N-API.

If any check fails, the result is unsupported.

## Platform Parsers

Implemented parser families:

- macOS arm64
- macOS x64
- Linux glibc arm64
- Linux glibc x64
- Windows arm64
- Windows x64
- Windows x86 (`ia32`, Node 20/22 only — Node ships no 32-bit Windows runtime after v22)

Linux musl is intentionally not published until its full runtime probing path is
implemented and CI-validated.

## Build Outputs

Local development output:

```text
packages/<family>/entry/build/<backend>/<abi>-<platform>/require_builtin.node
```

Platform prebuild output:

```text
packages/<family>/<platform>/prebuilt/<platform>-<binaryTag>.node
```

Examples:

```text
packages/internal-loader/entry/build/napi/napi-v9-darwin-arm64/require_builtin.node
packages/require-builtin/darwin-arm64/prebuilt/darwin-arm64-napi-v9.node
```

Generated binaries are ignored by git and produced by local release or CI jobs.

See [naming.md](naming.md) for project-wide environment variable, C++ symbol,
native binary, and source-file naming conventions.
