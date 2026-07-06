#ifndef INTERNAL_REQUIRE_RUNTIME_PROBE_HELPER_H_
#define INTERNAL_REQUIRE_RUNTIME_PROBE_HELPER_H_

#include "../runtime_compat.h"

#include <string_view>

namespace internal_require {

void* LookupProcessSymbol(std::string_view name);

template <typename Fn>
Fn LookupProcessFunction(std::string_view name) {
  // Private ABI assumption: the mangled symbol must still have the exact
  // calling convention encoded by Fn for the running Node/V8 build.
  return reinterpret_cast<Fn>(LookupProcessSymbol(name));
}

Result<GetterSymbol> ResolveBuiltinModuleRequireGetter(napi_env env, void* realm);
Result<ImageValidation> ValidateRuntimeImagePointers(void* realm, void* getter);
Result<GetterPattern> ParseBuiltinModuleRequireGetterOffset(void* getter);
Result<napi_value> ReadAndValidateRequireBuiltinHandle(napi_env env,
                                                       void* realm,
                                                       void* getter,
                                                       const GetterPattern& pattern);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_PROBE_HELPER_H_
