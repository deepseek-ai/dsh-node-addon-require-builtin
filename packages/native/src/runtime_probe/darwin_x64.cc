#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseDarwinX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX64SysVFieldGetter(getter, "darwin-x64");
}

}  // namespace esplus::node::require_builtin
