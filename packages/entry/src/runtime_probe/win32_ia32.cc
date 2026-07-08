#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseWin32Ia32BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX86ThiscallFieldGetter(getter, "win32-ia32");
}

}  // namespace esplus::node::require_builtin
