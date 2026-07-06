#include "helper.h"
#include "parser.h"

namespace internal_require {

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

}  // namespace internal_require
