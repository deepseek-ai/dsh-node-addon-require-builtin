#ifndef INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_
#define INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_

#include "../runtime_compat.h"

#include <string_view>

namespace internal_require {

Result<void*> ReadRealmVptr(void* realm);

void* LookupPlatformProcessSymbol(std::string_view name);
Result<GetterSymbol> ResolvePlatformBuiltinModuleRequireGetterFallback(
    napi_env env,
    void* realm);
Result<ImageValidation> ValidatePlatformRuntimeImagePointers(void* getter,
                                                             void* realm_vptr);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_
