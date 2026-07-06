#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseDarwinArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64FieldGetter(getter, "darwin-arm64");
}

}  // namespace internal_require
