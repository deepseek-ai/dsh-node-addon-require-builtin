#include "parser.h"

#include "getter_decoder.h"
#include "helper.h"

namespace esplus::node::require_builtin {

Result<GetterPattern> ParseLinuxGlibcArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  // The same matcher runs in both stages, so what differs between them is only
  // the window. Walking instead of matching fixed positions is what gives AArch64
  // the tolerance the x86-64 side always had, and it is why hardened x86-64 builds
  // were never affected by distribution hardening while AArch64 was.
  //
  // Because stage one walks, the hardened shapes that fit inside the standard
  // window are resolved there and never reach the retry: clang's
  // `paciasp; ldr; retaa` is 12 bytes and GCC's `paciasp; ldr; autiasp; ret` is
  // exactly 16, in either instruction order. The wider window is left to bodies
  // that carry a frame record, which is what pushes Fedora's getter to 28 bytes —
  // its build flags combine `-mbranch-protection=standard` with
  // `-mno-omit-leaf-frame-pointer`, so even this leaf accessor gets a frame.
  return DecodeGetterWithWideWindowRetry(getter, "linux-glibc-arm64",
                                         MatchArm64AapcsFieldGetter);
}

}  // namespace esplus::node::require_builtin
