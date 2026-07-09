#include "helper.h"

namespace esplus::node::require_builtin {

Result<CurrentContextRead> ReadLinuxGlibcArm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("linux-glibc-arm64", isolate, symbols);
}

}  // namespace esplus::node::require_builtin
