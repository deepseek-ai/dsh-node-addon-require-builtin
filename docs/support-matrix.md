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

CI runs the prebuilds against distribution-packaged Node on both architectures,
with no job allowed to fail softly: Fedora 44 (Node 20/22/24), Fedora rawhide
(22/24/26), Debian 13, Rocky 9, AlmaLinux 9, Amazon Linux 2023, openSUSE Leap 15.6
and Ubuntu 26.04. Each job prints the branch-protection census of the image that
contains V8, so which distributions harden their builds stays a measured fact.

Measured: Fedora is the only one whose getter needs the wider window, because it
alone pairs branch protection with `-mno-omit-leaf-frame-pointer` and so hardens
leaf functions (74839+ `paciasp`). Debian, Ubuntu 26.04, the RHEL 9 rebuilds and
Amazon Linux all enable branch protection as `pac-ret` without `+leaf`, giving a
few thousand `paciasp` in non-leaf functions and leaving this leaf accessor at
`bti c; ldr; ret`. Ubuntu 24.04 applies none at all and its Node is below this
project's floor.

Static linking is not by itself an obstacle: Rocky's `nodejs:20` and openSUSE's
Node have no shared `libnode` yet still export the private symbol, so the probe
resolves it. What cannot be supported is static linking *combined with* hidden
private symbols — Amazon Linux's default `nodejs` 18 is built that way, while its
versioned `nodejs20`/`nodejs22` packages use the shared layout and work.

The decoder's own self-test replays archived machine code from three toolchains
across an optimization and hardening matrix, plus bytes read out of shipped
binaries; see `scripts/collect-getter-fixtures.mjs`.

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
