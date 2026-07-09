# @esplus/node-addon-internal-loader

Published main package for the whitelisted `internal-loader` variant.

It exports the JavaScript API, selects a native binary through
`@esplus/node-addon-native-custom-loader`, and falls back to a local N-API
build when no current-platform optional package is usable.

This variant enforces an allow-list of internal module ids
(`internal/modules/cjs/loader` and `internal/modules/esm/loader`);
`requireBuiltin(id)` rejects any id outside it with
`Unsupported/disallowed-target`. For the unrestricted variant see
`@esplus/node-addon-require-builtin`.

See the repository [README](../../../README.md) for usage and support status.
