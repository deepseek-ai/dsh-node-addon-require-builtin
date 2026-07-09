# Naming

This document records project-wide naming conventions. Packaging mechanics live
in [packaging.md](packaging.md), runtime structure lives in
[architecture.md](architecture.md), and local workflow details live in
[development.md](development.md).

## npm Packages

Published npm packages are unscoped. There are two product families and one
shared loader package:

```text
node-addon-native-custom-loader
node-addon-require-builtin
node-addon-require-builtin-<platform>
node-addon-internal-loader
node-addon-internal-loader-<platform>
```

Entry package names contain the public product capability. Platform package
names contain the product family plus platform information. Backend and ABI
stay inside `prebuilds.json` and prebuilt filenames.

## Environment Variables

Runtime and build environment variables use the short project prefix `NARB_`,
standing for Node Addon Require Builtin. The prefix is kept stable across both
published product families:

```text
NARB_BACKEND
NARB_BUILD_OUTPUT
NARB_DISABLE_OPTIONAL_PACKAGE
NARB_DISABLE_LOCAL_BUILD
NARB_DISABLE_NATIVE_CACHE
NARB_EXPECTED_BACKEND
NARB_EXPECTED_PRODUCT
NARB_HEADERS_CACHE
NARB_NATIVE_CACHE_DIR
NARB_PRODUCT
NARB_TRACE
```

Do not include the npm scope in environment variable names.

## C/C++ Symbols

C/C++ preprocessor macros use the same `NARB_` prefix:

```cpp
NARB_BACKEND
NARB_BACKEND_NAPI
NARB_BACKEND_NODEABI
NARB_BACKEND_NODEABI_VERIFY
NARB_MEMBER_ABI
NARB_PRINTF_FORMAT
NARB_PRODUCT
NARB_PRODUCT_INTERNAL_LOADER
NARB_PRODUCT_REQUIRE_BUILTIN
```

C++ headers use `#pragma once` instead of include guards.

C++ implementation symbols live under the organization and feature namespace:

```cpp
namespace esplus::node::require_builtin {
}
```

## Native Binary And Sources

The native addon binary and node-gyp target are named after the public
capability, not the probing implementation:

```text
require_builtin.node
NODE_API_MODULE(require_builtin, Init)
```

Probe source files use `require_builtin_probe.*`. Avoid the old
`internal_require_probe.*` naming in new code.
