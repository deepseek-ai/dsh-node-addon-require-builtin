#pragma once

#include <string_view>

#define NARB_BACKEND_NAPI 1
#define NARB_BACKEND_NODEABI 2
#define NARB_BACKEND_NODEABI_VERIFY 3

#ifndef NARB_BACKEND
#define NARB_BACKEND NARB_BACKEND_NAPI
#endif

#if NARB_BACKEND != NARB_BACKEND_NAPI && \
    NARB_BACKEND != NARB_BACKEND_NODEABI &&  \
    NARB_BACKEND != NARB_BACKEND_NODEABI_VERIFY
#error "Unsupported NARB_BACKEND"
#endif

namespace esplus::node::require_builtin {

#if NARB_BACKEND == NARB_BACKEND_NAPI
constexpr std::string_view kBackend = "napi";
constexpr std::string_view kMode = "napi";
constexpr bool kUsesNapiBackend = true;
constexpr bool kUsesNodeBackend = false;
#elif NARB_BACKEND == NARB_BACKEND_NODEABI
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

}  // namespace esplus::node::require_builtin
