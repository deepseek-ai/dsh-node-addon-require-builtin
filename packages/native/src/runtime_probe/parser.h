#pragma once

#include "../runtime_compat.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseDarwinArm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseDarwinX64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseWin32Arm64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseWin32X64BuiltinModuleRequireGetterOffset(void* getter);
Result<GetterPattern> ParseWin32Ia32BuiltinModuleRequireGetterOffset(void* getter);

}  // namespace esplus::node::require_builtin
