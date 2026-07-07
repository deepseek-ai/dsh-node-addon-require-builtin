#ifndef INTERNAL_REQUIRE_RUNTIME_CONTEXT_HELPER_H_
#define INTERNAL_REQUIRE_RUNTIME_CONTEXT_HELPER_H_

#include "../runtime_compat.h"

namespace internal_require {

// A v8::Context exposes the realm in a fixed embedder slot. Any field count at
// or below that slot cannot hold a realm; an implausibly large one means the
// pointer is not a real context. Shared by every platform's context reader.
constexpr uint32_t kMaxPlausibleEmbedderFields = 4096;

using GetCurrentContextFn = void* (INTERNAL_REQUIRE_MEMBER_ABI*)(void*);
using GetNumberOfEmbedderDataFieldsFn =
    uint32_t (INTERNAL_REQUIRE_MEMBER_ABI*)(void*);

struct CurrentContextSymbols {
  void* get_current_context = nullptr;
  GetCurrentContextFn call_get_current_context = nullptr;
  GetNumberOfEmbedderDataFieldsFn get_fields = nullptr;
};

struct CurrentContextRead {
  void* context_ptr = nullptr;
  uint32_t embedder_fields = 0;
};

void TraceRuntimeContextCodeBytes(const char* label, void* fn);

bool IsEmbedderFieldCountPlausible(uint32_t embedder_fields);

Result<CurrentContextRead> ReadCurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadDirectCurrentV8Context(
    const char* platform,
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadSretCurrentV8Context(
    const char* platform,
    void* isolate,
    const CurrentContextSymbols& symbols);

Result<CurrentContextRead> ReadDarwinArm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadDarwinX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadLinuxGlibcArm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadLinuxGlibcX64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadWin32Arm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadWin32X64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);
Result<CurrentContextRead> ReadWin32Ia32CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_CONTEXT_HELPER_H_
