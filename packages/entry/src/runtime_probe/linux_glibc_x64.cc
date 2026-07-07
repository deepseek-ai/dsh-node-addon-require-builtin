#include "parser.h"

#include "getter_decoder.h"

namespace internal_require {

Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  return MatchX64SysVFieldGetter(getter, "linux-glibc-x64");
}

}  // namespace internal_require
