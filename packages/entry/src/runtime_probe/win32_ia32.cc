#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseWin32Ia32BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX86ThiscallFieldGetter(getter, "win32-ia32");
}

}  // namespace internal_require
