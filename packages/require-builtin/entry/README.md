# @esplus/node-addon-require-builtin

Published main package for the unrestricted `require-builtin` variant.

It exports the JavaScript API, selects a native binary through
`@esplus/node-addon-native-custom-loader`, and falls back to a local N-API
build when no current-platform optional package is usable.

This variant does not restrict internal module ids: `requireBuiltin(id)`
forwards any id to Node's builtin require, and `isAllowedInternalId()` always
returns `true`. For the whitelisted variant see
`@esplus/node-addon-internal-loader`.

See the repository [README](../../../README.md) for usage and support status.
