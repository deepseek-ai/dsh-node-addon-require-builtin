#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadWin32Arm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("win32-arm64", isolate, symbols);
}

}  // namespace internal_require
