# Support Matrix

The package is experimental and should be treated as best-effort before 1.0.

## Node Versions

The implemented target families are:

| Node major | `NODE_MODULE_VERSION` | Status |
|---|---:|---|
| 20 | 115 | Implemented |
| 22 | 127 | Implemented |
| 24 | 137 | Implemented |
| 26 | 147 | Implemented |

The N-API backend targets `napi-v9` across these majors.

The internal module allowlist is documented separately in
[internal-modules.md](internal-modules.md), including the supported semver
ranges for each Node major.

## Platform x Node Matrix

Supported optional prebuild packages:

| Platform suffix | Node 20 | Node 22 | Node 24 | Node 26 |
|---|---|---|---|---|
| `darwin-arm64` | Supported | Supported | Supported | Supported |
| `darwin-x64` | Supported | Supported | Supported | Supported |
| `linux-arm64-gnu` | Supported | Supported | Supported | Supported |
| `linux-x64-gnu` | Supported | Supported | Supported | Supported |
| `win32-arm64-msvc` | Supported | Supported | Supported | Supported |
| `win32-ia32-msvc` | Supported | Supported | No 32-bit runtime | No 32-bit runtime |
| `win32-x64-msvc` | Supported | Supported | Supported | Supported |

Node.js stopped publishing 32-bit Windows (`win-x86`) binaries after v22, so
`win32-ia32-msvc` is limited to Node 20 and 22; there is no v24/v26 32-bit
runtime to build against or test on.

Backend artifacts:

| Platform suffix | Optional package backend artifacts |
|---|---|
| `darwin-arm64` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |
| `darwin-x64` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |
| `linux-arm64-gnu` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |
| `linux-x64-gnu` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |
| `win32-arm64-msvc` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |
| `win32-ia32-msvc` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127` |
| `win32-x64-msvc` | `napi-v9`, `nodeabi-v115`, `nodeabi-v127`, `nodeabi-v137`, `nodeabi-v147` |

Not published yet:

| Platform suffix | Reason |
|---|---|
| `linux-arm64-musl` | musl getter parser not implemented |
| `linux-x64-musl` | musl getter parser not implemented |

Supported platform packages are exercised in CI with N-API and nodeabi optional
prebuild builds, optional package loading, local source-build fallback, and
require-parity against the genuine internals.

Unsupported runtimes should fail closed with diagnostics rather than loading an
unchecked internal module.
