#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchArm64FieldGetter(getter, "linux-glibc-arm64");
}

}  // namespace internal_require
