#include "helper.h"

namespace esplus::node::require_builtin {

Result<CurrentContextRead> ReadDarwinX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  return ReadDirectCurrentV8Context("darwin-x64", isolate, symbols);
}

}  // namespace esplus::node::require_builtin
