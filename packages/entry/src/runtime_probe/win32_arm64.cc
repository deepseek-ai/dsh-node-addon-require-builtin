#include "parser.h"

#include <array>
#include <cstring>

namespace internal_require {
namespace {

constexpr size_t kMaxReasonableRealmOffset = 0x4000;

Result<GetterPattern> Failure(const char* message) {
  return Result<GetterPattern>::Failure(
      Status::Failure(ProbeStatus::kUnsupportedNoGetter, message));
}

bool DecodeLdrX0FromX0(uint32_t ldr, size_t* offset) {
  constexpr uint32_t kMask = 0xffc003ffu;
  constexpr uint32_t kPattern = 0xf9400000u;
  if ((ldr & kMask) != kPattern) return false;
  *offset = static_cast<size_t>(((ldr >> 10) & 0xfffu) * sizeof(uint64_t));
  return true;
}

bool IsPlausibleOffset(size_t offset) {
  return offset != 0 &&
      offset <= kMaxReasonableRealmOffset &&
      (offset % alignof(void*)) == 0;
}

}  // namespace

Result<GetterPattern> ParseWin32Arm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  std::array<uint32_t, 3> code{};
  std::memcpy(code.data(), getter, code.size() * sizeof(code[0]));

  constexpr uint32_t kBtiC = 0xd503245fu;
  constexpr uint32_t kRet = 0xd65f03c0u;

  const size_t ldr_index = code[0] == kBtiC ? 1 : 0;

  size_t offset = 0;
  if (!DecodeLdrX0FromX0(code[ldr_index], &offset) ||
      code[ldr_index + 1] != kRet) {
    return Failure(
        "win32 arm64 getter machine code is not optional-bti-ldr-x0-this-imm-ret");
  }
  if (!IsPlausibleOffset(offset)) {
    return Failure("win32 arm64 parsed getter offset is implausible");
  }

  GetterPattern pattern;
  pattern.offset = offset;
  pattern.pattern = ldr_index == 0
      ? "win32-arm64 ldr-x0-this-imm-ret"
      : "win32-arm64 bti-c-ldr-x0-this-imm-ret";
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
