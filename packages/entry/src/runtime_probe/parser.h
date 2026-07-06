#ifndef INTERNAL_REQUIRE_RUNTIME_PROBE_PARSER_H_
#define INTERNAL_REQUIRE_RUNTIME_PROBE_PARSER_H_

#include "../runtime_compat.h"

namespace internal_require {

Result<GetterPattern> ParseDarwinArm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseDarwinX64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseWin32Arm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseWin32X64BuiltinModuleRequireGetterOffset(void* getter);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_PROBE_PARSER_H_
