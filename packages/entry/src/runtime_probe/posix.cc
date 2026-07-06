#if !defined(_WIN32)

#include "platform.h"

#include <dlfcn.h>

namespace internal_require {

void* LookupPlatformProcessSymbol(std::string_view name) {
  return dlsym(RTLD_DEFAULT, name.data());
}

Result<GetterSymbol> ResolvePlatformBuiltinModuleRequireGetterFallback(
    napi_env env,
    void* realm) {
  (void)env;
  (void)realm;
  return Result<GetterSymbol>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoGetter,
      "PrincipalRealm::builtin_module_require getter symbol not found"));
}

Result<ImageValidation> ValidatePlatformRuntimeImagePointers(void* getter,
                                                             void* realm_vptr) {
  ImageValidation image;
  image.vptr = reinterpret_cast<uintptr_t>(realm_vptr);

  Dl_info getter_info;
  Dl_info vptr_info;
  if (dladdr(getter, &getter_info) == 0 || getter_info.dli_fname == nullptr) {
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "getter address is not in a loaded image"));
  }
  if (dladdr(realm_vptr, &vptr_info) == 0 || vptr_info.dli_fname == nullptr) {
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
}

}  // namespace internal_require

#endif  // !defined(_WIN32)
