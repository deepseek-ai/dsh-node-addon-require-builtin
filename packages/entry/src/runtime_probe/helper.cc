#include "helper.h"

#include "parser.h"

#include <cstring>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

namespace internal_require {
namespace {

constexpr uintptr_t kMinPlausiblePointer = 0x10000;

// Private C++ ABI symbol. It is never linked at build time; every backend
// resolves it at runtime and validates the result before exposing it.
constexpr std::string_view kSymBuiltinModuleRequireGetter =
    "_ZNK4node14PrincipalRealm22builtin_module_requireEv";

using BuiltinModuleRequireGetterFn = napi_value (*)(void*);

}  // namespace

void* LookupProcessSymbol(std::string_view name) {
#if defined(_WIN32)
  (void)name;
  return nullptr;
#else
  return dlsym(RTLD_DEFAULT, name.data());
#endif
}

Result<GetterSymbol> ResolveBuiltinModuleRequireGetter() {
  void* getter = LookupProcessSymbol(kSymBuiltinModuleRequireGetter);
  if (getter == nullptr) {
    return Result<GetterSymbol>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "PrincipalRealm::builtin_module_require getter symbol not found"));
  }

  GetterSymbol symbol;
  symbol.address_ptr = getter;
  symbol.address = reinterpret_cast<uintptr_t>(getter);
  symbol.symbol = "found";
  symbol.symbol_name = std::string{kSymBuiltinModuleRequireGetter};
  return Result<GetterSymbol>::Ok(symbol);
}

Result<ImageValidation> ValidateRuntimeImagePointers(void* realm, void* getter) {
#if defined(_WIN32)
  (void)realm;
  (void)getter;
  return Result<ImageValidation>::Ok(ImageValidation{});
#else
  // Private object-layout check: Realm vptr is used only as evidence that the
  // pointer came from the same loaded Node image as the getter symbol.
  ImageValidation image;
  const uintptr_t realm_address = reinterpret_cast<uintptr_t>(realm);
  if (realm_address < kMinPlausiblePointer ||
      (realm_address % alignof(void*)) != 0) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm, "realm pointer is not plausible"));
  }

  void* vptr = nullptr;
  std::memcpy(&vptr, realm, sizeof(vptr));
  image.vptr = reinterpret_cast<uintptr_t>(vptr);
  if (!IsPointerAligned(image.vptr)) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr is null or unaligned"));
  }

  Dl_info getter_info;
  Dl_info vptr_info;
  if (dladdr(getter, &getter_info) == 0 || getter_info.dli_fname == nullptr) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "getter address is not in a loaded image"));
  }
  if (dladdr(vptr, &vptr_info) == 0 || vptr_info.dli_fname == nullptr) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr is not in a loaded image"));
  }

  image.getter_image = getter_info.dli_fname;
  image.vptr_image = vptr_info.dli_fname;
  if (getter_info.dli_fbase == nullptr ||
      getter_info.dli_fbase != vptr_info.dli_fbase) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr image does not match getter image"));
  }
  return Result<ImageValidation>::Ok(image);
#endif
}

Result<GetterPattern> ParseBuiltinModuleRequireGetterOffset(void* getter) {
#if defined(__APPLE__) && defined(__aarch64__)
  return ParseDarwinArm64BuiltinModuleRequireGetterOffset(getter);
#elif defined(__APPLE__) && defined(__x86_64__)
  return ParseDarwinX64BuiltinModuleRequireGetterOffset(getter);
#elif defined(__linux__) && defined(__GLIBC__) && defined(__aarch64__)
  return ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(getter);
#elif defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
  return ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(getter);
#elif defined(__linux__)
  return Result<GetterPattern>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoGetter,
      "unsupported linux libc getter parser"));
#elif defined(_WIN32) && defined(_M_ARM64)
  return ParseWin32Arm64BuiltinModuleRequireGetterOffset(getter);
#elif defined(_WIN32) && defined(_M_X64)
  return ParseWin32X64BuiltinModuleRequireGetterOffset(getter);
#else
  return Result<GetterPattern>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoGetter,
      "unsupported platform/architecture getter parser"));
#endif
}

Result<napi_value> ReadAndValidateRequireBuiltinHandle(napi_env env,
                                                       void* realm,
                                                       void* getter,
                                                       size_t offset) {
  napi_value candidate_from_field = nullptr;
  // Private layout read: Realm + parsed offset is expected to contain the same
  // handle word returned by PrincipalRealm::builtin_module_require().
  std::memcpy(&candidate_from_field,
              static_cast<const uint8_t*>(realm) + offset,
              sizeof(candidate_from_field));

  // Private ABI call: this executes a Node C++ getter that is not part of the
  // Node-API contract. The field read above must match this result.
  auto getter_fn = reinterpret_cast<BuiltinModuleRequireGetterFn>(getter);
  napi_value candidate_from_getter = getter_fn(realm);

  if (candidate_from_field != candidate_from_getter) {
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
    return Result<napi_value>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedHandleMode,
        "Realm field at parsed offset is not a function napi_value"));
  }

  return Result<napi_value>::Ok(candidate_from_getter);
}

}  // namespace internal_require
