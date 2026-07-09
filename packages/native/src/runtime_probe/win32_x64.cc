#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseWin32X64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX64Win64FieldGetter(getter, "win32-x64");
}

}  // namespace esplus::node::require_builtin
