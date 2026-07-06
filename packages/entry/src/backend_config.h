#ifndef INTERNAL_REQUIRE_BACKEND_CONFIG_H_
#define INTERNAL_REQUIRE_BACKEND_CONFIG_H_

#include <string_view>

#define INTERNAL_REQUIRE_BACKEND_NAPI 1
#define INTERNAL_REQUIRE_BACKEND_NODEABI 2
#define INTERNAL_REQUIRE_BACKEND_NODEABI_VERIFY 3

#ifndef INTERNAL_REQUIRE_BACKEND
#define INTERNAL_REQUIRE_BACKEND INTERNAL_REQUIRE_BACKEND_NAPI
#endif

#if INTERNAL_REQUIRE_BACKEND != INTERNAL_REQUIRE_BACKEND_NAPI && \
    INTERNAL_REQUIRE_BACKEND != INTERNAL_REQUIRE_BACKEND_NODEABI &&  \
    INTERNAL_REQUIRE_BACKEND != INTERNAL_REQUIRE_BACKEND_NODEABI_VERIFY
#error "Unsupported INTERNAL_REQUIRE_BACKEND"
#endif

namespace internal_require {

#if INTERNAL_REQUIRE_BACKEND == INTERNAL_REQUIRE_BACKEND_NAPI
constexpr std::string_view kBackend = "napi";
constexpr std::string_view kMode = "napi";
constexpr bool kUsesNapiBackend = true;
constexpr bool kUsesNodeBackend = false;
#elif INTERNAL_REQUIRE_BACKEND == INTERNAL_REQUIRE_BACKEND_NODEABI
constexpr std::string_view kBackend = "nodeabi";
constexpr std::string_view kMode = "nodeabi";
constexpr bool kUsesNapiBackend = false;
constexpr bool kUsesNodeAbiBackend = true;
#else
constexpr std::string_view kBackend = "nodeabi-verify";
constexpr std::string_view kMode = "nodeabi-verify";
constexpr bool kUsesNapiBackend = false;
constexpr bool kUsesNodeAbiBackend = true;
#endif

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_BACKEND_CONFIG_H_
