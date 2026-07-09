#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseWin32Arm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64Win64FieldGetter(getter, "win32-arm64");
}

}  // namespace esplus::node::require_builtin
