# @deepseek-ai/dsh-node-addon-internal-loader

Shared CommonJS loader used by `@deepseek-ai/dsh-node-addon-internal` and the platform
optional packages.

This package is published separately so platform packages can validate and load
their own prebuilt binaries without duplicating loader logic.
