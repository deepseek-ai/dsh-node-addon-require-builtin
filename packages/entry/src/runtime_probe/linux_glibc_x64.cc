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

Result<GetterPattern> ParseLinuxGlibcX64BuiltinModuleRequireGetterOffset(
    void* getter) {
  std::array<uint8_t, 8> code{};
  std::memcpy(code.data(), getter, code.size());

  GetterPattern pattern;
  if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x87 &&
      code[7] == 0xc3) {
    uint32_t disp = 0;
    std::memcpy(&disp, code.data() + 3, sizeof(disp));
    pattern.offset = disp;
    pattern.pattern = "linux-glibc-x64 mov-rax-this-rdi-disp32-ret";
  } else if (code[0] == 0x48 && code[1] == 0x8b && code[2] == 0x47 &&
             code[4] == 0xc3) {
    pattern.offset = code[3];
    pattern.pattern = "linux-glibc-x64 mov-rax-this-rdi-disp8-ret";
  } else {
    return Failure("linux glibc x64 getter machine code is not a supported getter");
  }

  if (!IsPlausibleOffset(pattern.offset)) {
    return Failure("linux glibc x64 parsed getter offset is implausible");
  }
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
