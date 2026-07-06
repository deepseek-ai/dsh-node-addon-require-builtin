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

Result<GetterPattern> ParseDarwinArm64BuiltinModuleRequireGetterOffset(
    void* getter) {
  std::array<uint32_t, 2> code{};
  std::memcpy(code.data(), getter, code.size() * sizeof(code[0]));

  constexpr uint32_t kRet = 0xd65f03c0u;

  size_t offset = 0;
  if (!DecodeLdrX0FromX0(code[0], &offset) || code[1] != kRet) {
    return Failure(
        "darwin arm64 getter machine code is not ldr-x0-this-imm-ret");
  }
  if (!IsPlausibleOffset(offset)) {
    return Failure("darwin arm64 parsed getter offset is implausible");
  }

  GetterPattern pattern;
  pattern.offset = offset;
  pattern.pattern = "darwin-arm64 ldr-x0-this-imm-ret";
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
