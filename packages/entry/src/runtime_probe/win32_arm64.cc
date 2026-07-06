#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseWin32Arm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64FieldGetter(getter, "win32-arm64");
}

}  // namespace internal_require
