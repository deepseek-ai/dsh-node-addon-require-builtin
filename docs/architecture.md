# Architecture

`@esplus/node-addon-require-builtin` is a Node native addon that obtains the bootstrap
`requireBuiltin()` function from the current Node `Realm` and exposes a small JS
API around it.

The addon has a stable public loading shell, but its core behavior is private
runtime probing:

```text
JS entry / optional package loader
  -> Node-API addon entry
  -> requireBuiltin decision flow
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

The published main package loads one native binary.

Selection order:

1. Resolve the current platform suffix, such as `darwin-arm64` or
   `linux-x64-gnu`.
2. Try the matching platform optional package unless
   `NARB_DISABLE_OPTIONAL_PACKAGE=1`.
3. In the optional package, load the `napi-v9` binary for the current platform.
4. If no optional package binary works, fail closed. Published packages do not
   compile native sources at install time.

`NARB_BACKEND=napi|nodeabi|auto` controls backend preference in development.
`auto` is the default.

## Native API Layer

`packages/entry/src/node_api_addon.cc` exports:

- `requireBuiltin(moduleId)`
- `isAllowedInternalId(moduleId)`
- `getNativeBindingInfo()`

The JS entry package re-exports only `requireBuiltin(moduleId)`,
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

`packages/entry/src/require_builtin_probe.cc` owns the backend-independent
control flow:

1. Ask the selected runtime adapter for a candidate `requireBuiltin` value.
2. Verify the JS function name is `requireBuiltin`.
3. Smoke test `requireBuiltin('internal/bootstrap/realm')`.
4. Verify the realm export self-reference points back to the same function.
5. Reject module ids outside the documented internal module allowlist.
6. Load the selected target internal module and record whether it returned
   exports. The getter does not inspect target export properties.

Target-load exceptions are converted into clear unsupported errors with
diagnostics. The allowlist is enforced in C++ before the private
`requireBuiltin()` value is resolved or called. See
[internal-modules.md](internal-modules.md) for the supported module ids and Node
version ranges.

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

`packages/entry/src/runtime_probe/helper.cc` receives an opaque `Realm*` from
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
packages/entry/build/<backend>/<abi>-<platform>/require_builtin.node
```

Platform prebuild output:

```text
packages/<platform>/prebuilt/<platform>-<binaryTag>.node
```

Examples:

```text
packages/darwin-arm64/prebuilt/darwin-arm64-napi-v9.node
```

Generated binaries are ignored by git and produced by local release or CI jobs.

See [naming.md](naming.md) for project-wide environment variable, C++ symbol,
native binary, and source-file naming conventions.
