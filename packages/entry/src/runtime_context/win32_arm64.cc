#include "helper.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace internal_require {
namespace {

constexpr uint32_t kArm64Ret = 0xd65f03c0u;

// Matches `str Xt, [x1, #imm]` (STR immediate, unsigned offset, 64-bit) with
// base register x1 — the hidden struct-return pointer under the MSVC arm64 ABI.
// A leaf getter stores the context straight through x1.
bool IsStoreThroughX1(uint32_t insn) {
  constexpr uint32_t kMask = 0xffc003e0u;     // opcode + Rn(x1), any Rt/imm
  constexpr uint32_t kPattern = 0xf9000020u;  // str xt, [x1, #imm]
  return (insn & kMask) == kPattern;
}

// Matches `mov Xd, x1` (the ORR Xd, XZR, x1 alias, no shift). A framed getter
// saves the struct-return pointer out of x1 into a callee-saved register before
// building its stack frame, so the reference to x1 shows up as a register move
// rather than an immediate store.
bool IsMoveFromX1(uint32_t insn) {
  constexpr uint32_t kMask = 0xffffffe0u;     // ORR Xd, XZR, x1 (shift=0), any Rd
  constexpr uint32_t kPattern = 0xaa0103e0u;  // mov xd, x1
  return (insn & kMask) == kPattern;
}

// A direct-return getter returns its v8::Local<Context> in x0 and never touches
// x1, whereas the MSVC sret ABI passes the return buffer in x1 and the body
// consumes it — either storing the context straight through x1 (leaf functions)
// or first saving x1 into a callee-saved register (framed functions). Scanning
// the prologue for either use of x1, before the first ret, confirms the symbol
// really uses the sret ABI, so we never issue a two-argument sret call against a
// misidentified direct-return function. Mirrors GetCurrentContextUsesRcxThis on
// win32-x64.
bool GetCurrentContextUsesX1Sret(void* fn) {
  constexpr size_t kWords = 16;  // 64 bytes of prologue is ample
  std::array<uint32_t, kWords> code{};
  std::memcpy(code.data(), fn, code.size() * sizeof(code[0]));
  for (size_t i = 0; i < code.size(); ++i) {
    if (code[i] == kArm64Ret) return false;
    if (IsStoreThroughX1(code[i]) || IsMoveFromX1(code[i])) return true;
  }
  return false;
}

}  // namespace

Result<CurrentContextRead> ReadWin32Arm64CurrentV8Context(
    void* isolate,
    const CurrentContextSymbols& symbols) {
  // MSVC returns v8::Local<Context> through a hidden struct-return pointer
  // (this=x0, return-buffer=x1), not directly in x0 like the Itanium ABI used
  // on darwin/linux arm64. Confirm the resolved symbol matches that shape
  // before calling it with the two-argument sret ABI.
  if (!GetCurrentContextUsesX1Sret(symbols.get_current_context)) {
    DebugTrace(
        "win32-arm64 GetCurrentContext prologue rejected: no store through "
        "sret x1");
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "win32-arm64 GetCurrentContext prologue does not match the sret ABI"));
  }
  DebugTrace("win32-arm64 GetCurrentContext ABI accepted: this=x0 sret=x1");
  return ReadSretCurrentV8Context("win32-arm64", isolate, symbols);
}

}  // namespace internal_require
