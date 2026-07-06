#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadDarwinArm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("darwin-arm64", isolate, symbols);
}

}  // namespace internal_require
