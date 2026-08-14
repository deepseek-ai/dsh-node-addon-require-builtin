#include "parser.h"

#include "getter_decoder.h"
#include "helper.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  // WalkX64Getter has always tolerated the hardened prologue and epilogue
  // instructions themselves (`endbr64`, a frame pointer, register shuffles), so
  // on x86-64 the window was the only limit that hardening could hit: `endbr64`
  // plus a frame pointer around a disp32 load is 17 bytes. Fedora's current
  // x86_64 build is a leaf function that fits well inside the standard window,
  // but that is a property of its compiler flags rather than something to rely
  // on, so the retry is wired up here the same way as on aarch64.
  return DecodeGetterWithWideWindowRetry(getter, "linux-glibc-x64",
                                         MatchX64SysVFieldGetter);
}

}  // namespace esplus::node::require_builtin
