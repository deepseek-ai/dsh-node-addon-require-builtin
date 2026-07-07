#include "helper.h"

#include <array>
#include <cstring>

namespace internal_require {
namespace {

using ContextSretFn = void (INTERNAL_REQUIRE_MEMBER_ABI*)(void* /*this*/,
                                                          void* /*ret*/);

bool GetCurrentContextUsesRcxThis(const uint8_t* code, size_t len) {
  size_t i = 0;
  if (len >= 4 && code[0] == 0xf3 && code[1] == 0x0f && code[2] == 0x1e &&
      code[3] == 0xfa) {
    i = 4;  // skip endbr64
  }
  for (; i + 3 <= len; ++i) {
    // 0x48 = REX.W, 0x8b = MOV r64, r/m64.
    if (code[i] != 0x48 || code[i + 1] != 0x8b) continue;
    const uint8_t modrm = code[i + 2];
    const uint8_t mod = modrm >> 6;
    const uint8_t rm = modrm & 0x7;
    // Memory operand (mod != 3) with base RCX (rm == 1) and not SIB/RIP.
    if (mod != 0x3 && rm == 0x1) return true;
  }
  return false;
}

}  // namespace

Result<CurrentContextRead> ReadWin32X64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  std::array<uint8_t, 24> prologue{};
  std::memcpy(prologue.data(), symbols.get_current_context, prologue.size());
  if (!GetCurrentContextUsesRcxThis(prologue.data(), prologue.size())) {
    DebugTrace(
        "win32-x64 GetCurrentContext prologue rejected: no early RCX-this load");
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "win32-x64 GetCurrentContext prologue does not match supported this/sret ABI"));
  }
  DebugTrace("win32-x64 GetCurrentContext ABI accepted: this=rcx sret=rdx");

  void* context = nullptr;
  DebugTrace("calling Isolate::GetCurrentContext(isolate) (win32-x64 sret)");
  reinterpret_cast<ContextSretFn>(symbols.get_current_context)(isolate, &context);
  const uintptr_t context_address = reinterpret_cast<uintptr_t>(context);
  DebugTrace("context=%s (win32-x64 this-rcx/sret-rdx)",
              Hex(context_address).c_str());
  if (!IsPointerAligned(context_address)) {
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "win32-x64 GetCurrentContext returned null or unaligned context"));
  }

  const uint32_t embedder_fields = symbols.get_fields(context);
  DebugTrace("embedder_fields=%u (realm_slot=%d)",
              embedder_fields, kRealmSlot);
  if (!IsEmbedderFieldCountPlausible(embedder_fields)) {
    DebugTrace("context rejected: implausible embedder fields=%u",
                embedder_fields);
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "win32-x64 context embedder field count is implausible"));
  }

  CurrentContextRead read;
  read.context_ptr = context;
  read.embedder_fields = embedder_fields;
  return Result<CurrentContextRead>::Ok(read);
}

}  // namespace internal_require
