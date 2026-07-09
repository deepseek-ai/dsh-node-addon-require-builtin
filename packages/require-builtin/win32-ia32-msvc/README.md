# node-addon-require-builtin-win32-ia32-msvc

Optional prebuilt native addon package for `win32-ia32-msvc` (32-bit x86).

The main `node-addon-require-builtin` package selects one binary from
this package at runtime using `prebuilds.json`.

Node.js ships no 32-bit Windows runtime after v22, so this package targets
Node 20 and 22 only with `napi-v9`.
