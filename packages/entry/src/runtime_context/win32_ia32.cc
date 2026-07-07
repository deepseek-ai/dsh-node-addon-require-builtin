#include "helper.h"

namespace internal_require {

Result<CurrentContextRead> ReadWin32Ia32CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  // MSVC returns a non-trivial v8::Local<Context> through a hidden struct-return
  // pointer under __thiscall: `this` is in ECX and the return-buffer pointer is
  // the first pushed stack argument, which the callee cleans up. The shared sret
  // reader calls GetCurrentContext through a __thiscall two-argument pointer, so
  // the buffer is supplied and the stack stays balanced — unlike the Itanium
  // direct-return path used on darwin/linux.
  return ReadSretCurrentV8Context("win32-ia32", isolate, symbols);
}

}  // namespace internal_require
