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

## Platforms

Published prebuild packages:

| Platform suffix | Status |
|---|---|
| `darwin-arm64` | Implemented |
| `darwin-x64` | Implemented |
| `linux-arm64-gnu` | Implemented |
| `linux-x64-gnu` | Implemented |

Not published yet:

| Platform suffix | Reason |
|---|---|
| `linux-arm64-musl` | musl getter parser not implemented |
| `linux-x64-musl` | musl getter parser not implemented |
| `win32-arm64-msvc` | Windows symbol/image lookup not implemented |
| `win32-x64-msvc` | Windows symbol/image lookup not implemented |

Unsupported runtimes should fail closed with diagnostics rather than loading an
unchecked internal module.
