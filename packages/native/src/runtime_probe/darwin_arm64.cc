#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseDarwinArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64FieldGetter(getter, "darwin-arm64");
}

}  // namespace esplus::node::require_builtin
