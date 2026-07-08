#include "helper.h"

namespace esplus::node::require_builtin {

Result<CurrentContextRead> ReadLinuxGlibcX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("linux-glibc-x64", isolate, symbols);
}

}  // namespace esplus::node::require_builtin
