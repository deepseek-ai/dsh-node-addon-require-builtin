# node-addon-require-builtin-loader

Shared CommonJS loader used by `node-addon-require-builtin` and the platform
optional packages.

This package is published separately so platform packages can validate and load
their own prebuilt binaries without duplicating loader logic.
