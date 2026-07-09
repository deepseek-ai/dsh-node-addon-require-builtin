#pragma once

#include <string_view>

// Product variant selector. One source tree compiles into two published
// families, chosen at build time by -DNARB_PRODUCT (mirrors the NARB_BACKEND
// pattern in backend_config.h):
//   NARB_PRODUCT_REQUIRE_BUILTIN — no internal-id whitelist; requireBuiltin()
//     forwards any id to Node's builtin require and isAllowedInternalId()
//     always returns true.
//   NARB_PRODUCT_INTERNAL_LOADER — enforces the cjs/esm loader whitelist.
// The default is the restricted variant so an unconfigured build fails closed.
#define NARB_PRODUCT_REQUIRE_BUILTIN 1
#define NARB_PRODUCT_INTERNAL_LOADER 2

#ifndef NARB_PRODUCT
#define NARB_PRODUCT NARB_PRODUCT_INTERNAL_LOADER
#endif

#if NARB_PRODUCT != NARB_PRODUCT_REQUIRE_BUILTIN && \
    NARB_PRODUCT != NARB_PRODUCT_INTERNAL_LOADER
#error "Unsupported NARB_PRODUCT"
#endif

namespace esplus::node::require_builtin {

#if NARB_PRODUCT == NARB_PRODUCT_REQUIRE_BUILTIN
constexpr std::string_view kProduct = "require-builtin";
constexpr std::string_view kProductPackageName =
    "@esplus/node-addon-require-builtin";
constexpr bool kEnforceWhitelist = false;
#else
constexpr std::string_view kProduct = "internal-loader";
constexpr std::string_view kProductPackageName =
    "@esplus/node-addon-internal-loader";
constexpr bool kEnforceWhitelist = true;
#endif

}  // namespace esplus::node::require_builtin
