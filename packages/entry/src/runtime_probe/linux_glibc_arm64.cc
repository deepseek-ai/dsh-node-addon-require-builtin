#include "parser.h"

#include "getter_decoder.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64FieldGetter(getter, "linux-glibc-arm64");
}

}  // namespace esplus::node::require_builtin
