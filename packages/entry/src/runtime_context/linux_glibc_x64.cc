#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadLinuxGlibcX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("linux-glibc-x64", isolate, symbols);
}

}  // namespace internal_require
