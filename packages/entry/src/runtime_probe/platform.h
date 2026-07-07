#ifndef INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_
#define INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_

#include "../runtime_compat.h"

#include <string_view>

namespace internal_require {

Result<void*> ReadRealmVptr(void* realm);

// Returns true only if the whole [address, address+size) range is currently
// mapped and readable in this process. Used to gate every dereference of a
// runtime-derived pointer so a bad probe fails closed instead of faulting.
bool IsPlatformReadableRange(const void* address, size_t size);

void* LookupPlatformProcessSymbol(std::string_view name);
Result<GetterSymbol> ResolvePlatformBuiltinModuleRequireGetterFallback(
    napi_env env,
    void* realm);
Result<ImageValidation> ValidatePlatformRuntimeImagePointers(void* getter,
                                                             void* realm_vptr);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_PROBE_PLATFORM_H_
