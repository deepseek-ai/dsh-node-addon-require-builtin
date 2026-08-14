#pragma once

#include "../runtime_compat.h"

#include <string_view>

namespace esplus::node::require_builtin {

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

// A field-getter matcher that decodes at most `window_bytes` from the getter.
// Both linux matchers share this signature, which is what lets the two-stage
// decode below be one implementation instead of two parallel copies.
using WindowedFieldGetterMatcher = Result<GetterPattern> (*)(void*,
                                                             std::string_view,
                                                             size_t);

// Decodes a getter in two stages: the standard window first, then the wider
// hardened window only if that failed.
//
// Linux distributions compile Node with hardening flags the official binaries do
// not use, which wrap the same one-instruction field load in a longer prologue
// and epilogue and can push the terminating instruction past
// kGetterCodeWindowBytes. Retrying in the wider window is what accommodates that,
// and it is deliberately a retry rather than a bigger default: a getter that
// already matched in the standard window takes exactly the path it always did.
//
// The wider window is only attempted once those extra bytes are known to be
// mapped, so a getter near the end of a mapping fails with the standard-window
// diagnostic rather than faulting. When both stages fail, both reasons are
// reported — on a hardened binary it is the second that names what was actually
// unrecognized, so reporting only the first would point debugging at the wrong
// stage.
//
// Only the linux parsers use this. darwin and win32 Node binaries come from
// official builds with no third-party recompilation ecosystem applying hardening
// flags, and win32 additionally feeds arbitrary vtable-scan candidates to its
// matcher under a documented 16-byte readability contract (see
// kGetterCodeWindowBytes) that widening would require re-auditing.
Result<GetterPattern> DecodeGetterWithWideWindowRetry(
    void* getter,
    std::string_view platform_tag,
    WindowedFieldGetterMatcher match);

}  // namespace esplus::node::require_builtin
