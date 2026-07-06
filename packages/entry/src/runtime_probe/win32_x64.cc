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
  std::array<uint8_t, 16> code{};
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
  } else if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x81 &&
             code[7] == 0x48 && code[8] == 0x89 && code[9] == 0x02 &&
             code[10] == 0x48 && code[11] == 0x8b && code[12] == 0xc2 &&
             code[13] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 3, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern = "win32-x64 mov-rax-this-rcx-disp32-store-sret-ret";
    pattern.call_mode = GetterCallMode::kSret;
  } else if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x41 &&
             code[4] == 0x48 && code[5] == 0x89 && code[6] == 0x02 &&
             code[7] == 0x48 && code[8] == 0x8b && code[9] == 0xc2 &&
             code[10] == 0xc3) {
    pattern.offset = code[3];
    pattern.pattern = "win32-x64 mov-rax-this-rcx-disp8-store-sret-ret";
    pattern.call_mode = GetterCallMode::kSret;
  } else if (code[0] == 0x48 && code[1] == 0x89 && code[2] == 0xd0 &&
             code[3] == 0x48 && code[4] == 0x8b && code[5] == 0x89 &&
             code[10] == 0x48 && code[11] == 0x89 && code[12] == 0x0a &&
             code[13] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 6, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern =
        "win32-x64 mov-rax-sret-rdx-mov-rcx-this-rcx-disp32-store-sret-ret";
    pattern.call_mode = GetterCallMode::kSret;
  } else if (code[0] == 0x48 && code[1] == 0x89 && code[2] == 0xd0 &&
             code[3] == 0x48 && code[4] == 0x8b && code[5] == 0x49 &&
             code[7] == 0x48 && code[8] == 0x89 && code[9] == 0x0a &&
             code[10] == 0xc3) {
    pattern.offset = code[6];
    pattern.pattern =
        "win32-x64 mov-rax-sret-rdx-mov-rcx-this-rcx-disp8-store-sret-ret";
    pattern.call_mode = GetterCallMode::kSret;
  } else {
    return Failure("win32 x64 getter machine code is not a supported getter");
  }

  if (!IsPlausibleOffset(pattern.offset)) {
    return Failure("win32 x64 parsed getter offset is implausible");
  }
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
