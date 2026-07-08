#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX64SysVFieldGetter(getter, "linux-glibc-x64");
}

}  // namespace esplus::node::require_builtin
