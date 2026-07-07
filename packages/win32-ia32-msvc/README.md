# @deepseek-ai/dsh-node-addon-internal-win32-ia32-msvc

Optional prebuilt native addon package for `win32-ia32-msvc` (32-bit x86).

The main `@deepseek-ai/dsh-node-addon-internal` package selects one binary from this
package at runtime using `prebuilds.json`.

Node.js ships no 32-bit Windows runtime after v22, so this package targets
Node 20 and 22 only (`napi-v9`, `nodeabi-v115`, `nodeabi-v127`).
