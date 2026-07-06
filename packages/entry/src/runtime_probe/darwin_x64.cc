#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseDarwinX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX64SysVFieldGetter(getter, "darwin-x64");
}

}  // namespace internal_require
