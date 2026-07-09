# Internal Module Allowlist

`node-addon-internal-loader` exposes only a small allowlist of Node
internal modules. The list is enforced in the native addon before Node's
private `requireBuiltin()` value is called.

`node-addon-require-builtin` is the unrestricted product family. It does
not use this allowlist; `requireBuiltin(id)` forwards any string id to Node.

## Supported Internal Modules

| Module id | Status | Supported Node ranges |
|---|---|---|
| `internal/modules/cjs/loader` | Supported | `>=20.0.0 <21.0.0`, `>=22.0.0 <23.0.0`, `>=24.0.0 <25.0.0`, `>=26.0.0 <27.0.0` |
| `internal/modules/esm/loader` | Supported | `>=20.0.0 <21.0.0`, `>=22.0.0 <23.0.0`, `>=24.0.0 <25.0.0`, `>=26.0.0 <27.0.0` |

Node odd-numbered release lines and other internal module ids are not supported
by the `internal-loader` support matrix. A request for any module id outside
this list throws an `Unsupported/disallowed-target` error in the
`internal-loader` product.

## Version And ABI Coverage

| Node range | `NODE_MODULE_VERSION` | Published artifact |
|---|---:|---|
| `>=20.0.0 <21.0.0` | 115 | `napi-v9` |
| `>=22.0.0 <23.0.0` | 127 | `napi-v9` |
| `>=24.0.0 <25.0.0` | 137 | `napi-v9` |
| `>=26.0.0 <27.0.0` | 147 | `napi-v9` |

The `nodeabi` backend remains available for repository source-build validation,
but nodeabi binaries are not published.

Windows x86 (`win32-ia32-msvc`) is limited to Node 20 and Node 22 because Node.js
does not publish 32-bit Windows runtimes after v22.

## Support Meaning

Supported means `internal-loader` may load the internal module when all of these
conditions are true:

- The current platform and Node line are in the published support matrix.
- The selected native backend passes runtime probing and smoke tests.
- The requested module id is listed above.
- The target module returns exports from Node's genuine internal module cache.

These packages do not make these Node internals public API. Export shapes,
object identities, and behaviors remain Node implementation details and may
change across Node releases.

See [support-matrix.md](support-matrix.md) for platform coverage.
