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

bool IsPlausibleOffset(size_t offset) {
  return offset != 0 &&
      offset <= kMaxReasonableRealmOffset &&
      (offset % alignof(void*)) == 0;
}

}  // namespace

Result<GetterPattern> ParseWin32X64BuiltinModuleRequireGetterOffset(
    void* getter) {
  std::array<uint8_t, 8> code{};
  std::memcpy(code.data(), getter, code.size());

  GetterPattern pattern;
  if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x81 &&
      code[7] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 3, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern = "win32-x64 mov-rax-this-rcx-disp32-ret";
  } else if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x41 &&
             code[4] == 0xc3) {
    pattern.offset = code[3];
    pattern.pattern = "win32-x64 mov-rax-this-rcx-disp8-ret";
  } else {
    return Failure("win32 x64 getter machine code is not a supported getter");
  }

  if (!IsPlausibleOffset(pattern.offset)) {
    return Failure("win32 x64 parsed getter offset is implausible");
  }
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
