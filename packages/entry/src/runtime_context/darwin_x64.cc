#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadDarwinX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("darwin-x64", isolate, symbols);
}

}  // namespace internal_require
