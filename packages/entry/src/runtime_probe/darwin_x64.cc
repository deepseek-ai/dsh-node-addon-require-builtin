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

Result<GetterPattern> ParseDarwinX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  std::array<uint8_t, 16> code{};
  std::memcpy(code.data(), getter, code.size());

  GetterPattern pattern;
  if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x87 &&
      code[7] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 3, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern = "darwin-x64 mov-rax-this-rdi-disp32-ret";
  } else if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x47 &&
             code[4] == 0xc3) {
    pattern.offset = code[3];
    pattern.pattern = "darwin-x64 mov-rax-this-rdi-disp8-ret";
  } else if (
      // Newer x64 release builds may keep a frame pointer around the getter:
      // push rbp; mov rsp, rbp; mov offset(rdi), rax; pop rbp; ret.
      code[0] == 0x55 && code[1] == 0x48 && code[2] == 0x89 &&
      code[3] == 0xe5 && code[4] == 0x48 && code[5] == 0x8b &&
      code[6] == 0x87 && code[11] == 0x5d && code[12] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 7, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern =
        "darwin-x64 push-rbp-mov-rsp-rbp-mov-rax-this-rdi-disp32-pop-rbp-ret";
  } else if (code[0] == 0x55 && code[1] == 0x48 && code[2] == 0x89 &&
             code[3] == 0xe5 && code[4] == 0x48 && code[5] == 0x8b &&
             code[6] == 0x47 && code[8] == 0x5d && code[9] == 0xc3) {
    pattern.offset = code[7];
    pattern.pattern =
        "darwin-x64 push-rbp-mov-rsp-rbp-mov-rax-this-rdi-disp8-pop-rbp-ret";
  } else {
    return Failure("darwin x64 getter machine code is not a supported getter");
  }

  if (!IsPlausibleOffset(pattern.offset)) {
    return Failure("darwin x64 parsed getter offset is implausible");
  }
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
