# @esplus/node-addon-require-builtin-loader

Shared CommonJS loader used by `@esplus/node-addon-require-builtin` and the
platform optional packages.

This package is published separately so platform packages can validate and load
their own prebuilt binaries without duplicating loader logic.

Native `.node` files are copied to a content-addressed runtime cache before
loading. This keeps package-managed files replaceable while a process is
running, especially on Windows where loaded DLLs are locked.
