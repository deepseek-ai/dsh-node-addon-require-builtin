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

Both product families publish the same supported optional prebuild package
matrix:

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

The Linux GNU prebuilds target glibc 2.28 and explicitly link `libdl.so.2`.
They share the process C++ runtime instead of embedding another `libstdc++` or
`libgcc`. CI builds and loads them against the manylinux 2.28 system libraries
and rejects artifacts requiring newer than `GLIBC_2.28` or `GLIBCXX_3.4.25`.

Distribution-packaged Node is also covered on Linux. It is not merely a
different version: Fedora ships Node as a thin `/usr/bin/node` linked against a
shared `libnode.so`, compiled with hardening the nodejs.org binaries do not use.
On aarch64 its `%{build_cflags}` combine `-mbranch-protection=standard` with
`-mno-omit-leaf-frame-pointer`, which give even this leaf accessor pointer
authentication and a frame record — a 28-byte body where the official build emits
8 to 12 bytes. Both linux platforms therefore decode the getter in two stages,
using the same instruction-walking matcher in each: the standard 16-byte window
first, then a wider 32-byte window for bodies that do not terminate inside it.

CI runs the prebuilds against Fedora's own packages on both architectures,
covering Node 20/22/24 on Fedora 44 and Node 22/24/26 on rawhide (neither release
carries all four streams). Each job prints the branch-protection census of the
`libnode` it installed, so which distributions harden their builds stays a
measured fact rather than an assumption — Ubuntu 24.04, for instance, applies no
branch protection at all and its Node is below this project's floor. The decoder's
own self-test replays archived machine code from three toolchains across an
optimization and hardening matrix, plus bytes read out of shipped binaries; see
`scripts/collect-getter-fixtures.mjs`.

Windows x64 and x86 prebuilds are built and tested on the Windows Server 2022
runner; ARM64 uses the Windows 11 ARM runner. All Windows builds pin Visual
Studio 2022 and CI rejects dynamic MSVC/UCRT runtime dependencies.

Published backend artifacts for each product family:

| Platform suffix | Optional package backend artifacts |
|---|---|
| `darwin-arm64` | `napi-v9` |
| `darwin-x64` | `napi-v9` |
| `linux-arm64-gnu` | `napi-v9` |
| `linux-x64-gnu` | `napi-v9` |
| `win32-arm64-msvc` | `napi-v9` |
| `win32-ia32-msvc` | `napi-v9` |
| `win32-x64-msvc` | `napi-v9` |

Not published yet:

| Platform suffix | Reason |
|---|---|
| `linux-arm64-musl` | musl getter parser not implemented |
| `linux-x64-musl` | musl getter parser not implemented |

Supported platform packages are exercised in CI with N-API optional prebuild
builds across supported Node majors, both product families, optional package
loading, repository source builds, nodeabi source-build validation, and
require-parity against the genuine internals.

Unsupported runtimes should fail closed with diagnostics rather than loading an
unchecked internal module.
