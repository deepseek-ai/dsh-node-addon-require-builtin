#include "helper.h"

#include "platform.h"

#include <cstring>

namespace internal_require {
namespace {

constexpr uintptr_t kMinPlausiblePointer = 0x10000;

// Private C++ ABI symbol. It is never linked at build time; every backend
// resolves it at runtime and validates the result before exposing it.
constexpr std::string_view kSymBuiltinModuleRequireGetter =
    "_ZNK4node14PrincipalRealm22builtin_module_requireEv";

using BuiltinModuleRequireGetterFn = napi_value (*)(void*);
using BuiltinModuleRequireGetterSretFn = void (*)(void*, void*);

}  // namespace

Result<void*> ReadRealmVptr(void* realm) {
  const uintptr_t realm_address = reinterpret_cast<uintptr_t>(realm);
  TracePrintf("reading Realm vptr from realm=%s", Hex(realm_address).c_str());
  if (realm_address < kMinPlausiblePointer ||
      (realm_address % alignof(void*)) != 0) {
    return Result<void*>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm, "realm pointer is not plausible"));
  }

  void* vptr = nullptr;
  std::memcpy(&vptr, realm, sizeof(vptr));
  if (!IsPointerAligned(reinterpret_cast<uintptr_t>(vptr))) {
    return Result<void*>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr is null or unaligned"));
  }
  TracePrintf("Realm vptr=%s",
              Hex(reinterpret_cast<uintptr_t>(vptr)).c_str());
  return Result<void*>::Ok(vptr);
}

void* LookupProcessSymbol(std::string_view name) {
  return LookupPlatformProcessSymbol(name);
}

Result<GetterSymbol> ResolveBuiltinModuleRequireGetter(napi_env env,
                                                       void* realm) {
  void* getter = LookupProcessSymbol(kSymBuiltinModuleRequireGetter);
  TracePrintf("LookupProcessSymbol(builtin_module_require getter) -> %p", getter);
  if (getter == nullptr) {
    return ResolvePlatformBuiltinModuleRequireGetterFallback(env, realm);
  }

  GetterSymbol symbol;
  symbol.address_ptr = getter;
  symbol.address = reinterpret_cast<uintptr_t>(getter);
  symbol.symbol = "found";
  symbol.symbol_name = std::string{kSymBuiltinModuleRequireGetter};
  return Result<GetterSymbol>::Ok(symbol);
}

Result<ImageValidation> ValidateRuntimeImagePointers(void* realm, void* getter) {
  // Private object-layout check: Realm vptr is used only as evidence that the
  // pointer came from the same loaded Node image as the getter symbol.
  auto vptr = ReadRealmVptr(realm);
  if (!vptr.ok()) {
    return Result<ImageValidation>::Failure(vptr.status());
  }
  return ValidatePlatformRuntimeImagePointers(getter, vptr.value());
}

Result<napi_value> ReadAndValidateRequireBuiltinHandle(napi_env env,
                                                       void* realm,
                                                       void* getter,
                                                       const GetterPattern& pattern) {
  napi_value candidate_from_field = nullptr;
  // Private layout read: Realm + parsed offset is expected to contain the same
  // handle word returned by PrincipalRealm::builtin_module_require().
  std::memcpy(&candidate_from_field,
              static_cast<const uint8_t*>(realm) + pattern.offset,
              sizeof(candidate_from_field));
  TracePrintf("requireBuiltin field read: realm=%s offset=%s value=%s",
              Hex(reinterpret_cast<uintptr_t>(realm)).c_str(),
              Hex(pattern.offset).c_str(),
              Hex(reinterpret_cast<uintptr_t>(candidate_from_field)).c_str());

  // Private ABI call: this executes a Node C++ getter that is not part of the
  // Node-API contract. The field read above must match this result.
  napi_value candidate_from_getter = nullptr;
  if (pattern.call_mode == GetterCallMode::kSret) {
    auto getter_fn = reinterpret_cast<BuiltinModuleRequireGetterSretFn>(getter);
    getter_fn(realm, &candidate_from_getter);
  } else {
    auto getter_fn = reinterpret_cast<BuiltinModuleRequireGetterFn>(getter);
    candidate_from_getter = getter_fn(realm);
  }
  TracePrintf("requireBuiltin getter call: getter=%s mode=%s value=%s",
              Hex(reinterpret_cast<uintptr_t>(getter)).c_str(),
              pattern.call_mode == GetterCallMode::kSret ? "sret" : "direct",
              Hex(reinterpret_cast<uintptr_t>(candidate_from_getter)).c_str());

  if (candidate_from_field != candidate_from_getter) {
    TracePrintf("requireBuiltin handle mismatch: field=%s getter=%s",
                Hex(reinterpret_cast<uintptr_t>(candidate_from_field)).c_str(),
                Hex(reinterpret_cast<uintptr_t>(candidate_from_getter)).c_str());
    return Result<napi_value>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedHandleMode,
        "getter result did not match field value at parsed offset"));
  }

  napi_valuetype type = napi_undefined;
  // Private-to-public handle bridge: Node's V8-backed Node-API represents
  // napi_value as the same handle word as v8::Local<Value>. N-API validation is
  // used immediately so a mismatched representation fails closed.
  if (candidate_from_getter == nullptr ||
      napi_typeof(env, candidate_from_getter, &type) != napi_ok ||
      type != napi_function) {
    TracePrintf("requireBuiltin handle type validation failed: value=%s type=%d",
                Hex(reinterpret_cast<uintptr_t>(candidate_from_getter)).c_str(),
                static_cast<int>(type));
    return Result<napi_value>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedHandleMode,
        "Realm field at parsed offset is not a function napi_value"));
  }

  return Result<napi_value>::Ok(candidate_from_getter);
}

}  // namespace internal_require
