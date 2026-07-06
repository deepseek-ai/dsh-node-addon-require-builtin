#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadLinuxGlibcArm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("linux-glibc-arm64", isolate, symbols);
}

}  // namespace internal_require
