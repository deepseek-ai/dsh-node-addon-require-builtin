# node-addon-internal-loader

Published entry package for the whitelisted `internal-loader` variant.

It exports the JavaScript API, selects a native binary through
`node-addon-native-custom-loader`, and uses a repository-only local
N-API source fallback when no current-platform optional package is usable.
Published installs do not ship native sources and fail closed instead of
compiling unvalidated local binaries.

This variant enforces an allow-list of internal module ids
(`internal/modules/cjs/loader` and `internal/modules/esm/loader`);
`requireBuiltin(id)` rejects any id outside it with
`Unsupported/disallowed-target`. For the unrestricted variant see
`node-addon-require-builtin`.

See the repository [README](../../../README.md) for usage and support status.
