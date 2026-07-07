#include "helper.h"

namespace internal_require {
namespace {

Result<CurrentContextRead> UnsupportedContextReader(const char* message) {
  return Result<CurrentContextRead>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoContext, message));
}

bool HasRequiredSymbols(const CurrentContextSymbols& symbols) {
  return symbols.get_current_context != nullptr &&
      symbols.call_get_current_context != nullptr &&
      symbols.get_fields != nullptr;
}

}  // namespace

Result<CurrentContextRead> ReadCurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  if (isolate == nullptr || !HasRequiredSymbols(symbols)) {
    return UnsupportedContextReader(
        "current-context reader was called without required symbols");
  }

#if defined(__APPLE__) && defined(__aarch64__)
  return ReadDarwinArm64CurrentV8Context(isolate, symbols);
#elif defined(__APPLE__) && defined(__x86_64__)
  return ReadDarwinX64CurrentV8Context(isolate, symbols);
#elif defined(__linux__) && defined(__GLIBC__) && defined(__aarch64__)
  return ReadLinuxGlibcArm64CurrentV8Context(isolate, symbols);
#elif defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
  return ReadLinuxGlibcX64CurrentV8Context(isolate, symbols);
#elif defined(__linux__)
  return UnsupportedContextReader("unsupported linux libc current-context reader");
#elif defined(_WIN32) && defined(_M_ARM64)
  return ReadWin32Arm64CurrentV8Context(isolate, symbols);
#elif defined(_WIN32) && defined(_M_X64)
  return ReadWin32X64CurrentV8Context(isolate, symbols);
#elif defined(_WIN32) && defined(_M_IX86)
  return ReadWin32Ia32CurrentV8Context(isolate, symbols);
#else
  return UnsupportedContextReader(
      "unsupported platform/architecture current-context reader");
#endif
}

}  // namespace internal_require
