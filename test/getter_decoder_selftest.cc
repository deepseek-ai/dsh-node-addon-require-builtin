// Standalone verification for the x64/arm64 getter decoders. Feeds each decoder
// the exact machine-code shapes the per-platform parsers recognize and asserts
// the decoded offset and call mode. The decoders are host-architecture
// independent, so this runs on any build host regardless of the target it
// decodes for. Compiled and run by scripts/test-getter-decoder.ts.
#include "runtime_probe/getter_decoder.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using internal_require::GetterCallMode;
using internal_require::GetterPattern;
using internal_require::MatchArm64FieldGetter;
using internal_require::MatchArm64Win64FieldGetter;
using internal_require::MatchX64SysVFieldGetter;
using internal_require::MatchX64Win64FieldGetter;
using internal_require::MatchX86ThiscallFieldGetter;
using internal_require::Result;

namespace {

int g_failures = 0;

void ExpectOk(const char* name,
              Result<GetterPattern> result,
              size_t expected_offset,
              GetterCallMode expected_mode) {
  if (!result.ok()) {
    std::printf("FAIL %-52s parse rejected: %s\n", name,
                result.status().message().c_str());
    g_failures++;
    return;
  }
  const GetterPattern& pattern = result.value();
  if (pattern.offset != expected_offset) {
    std::printf("FAIL %-52s offset=0x%zx expected=0x%zx\n", name,
                pattern.offset, expected_offset);
    g_failures++;
    return;
  }
  if (pattern.call_mode != expected_mode) {
    std::printf("FAIL %-52s call_mode mismatch\n", name);
    g_failures++;
    return;
  }
  std::printf("ok   %-52s offset=0x%zx [%s]\n", name, pattern.offset,
              pattern.pattern.c_str());
}

void ExpectReject(const char* name, Result<GetterPattern> result) {
  if (result.ok()) {
    std::printf("FAIL %-52s expected rejection, got offset=0x%zx\n", name,
                result.value().offset);
    g_failures++;
    return;
  }
  std::printf("ok   %-52s rejected as expected\n", name);
}

std::vector<uint8_t> Pad16(std::vector<uint8_t> bytes) {
  bytes.resize(16, 0x00);
  return bytes;
}

// Little-endian disp32 helper.
void PushDisp32(std::vector<uint8_t>* out, uint32_t value) {
  out->push_back(value & 0xff);
  out->push_back((value >> 8) & 0xff);
  out->push_back((value >> 16) & 0xff);
  out->push_back((value >> 24) & 0xff);
}

std::vector<uint32_t> Arm64Words(std::vector<uint32_t> words) {
  words.resize(4, 0);
  return words;
}

}  // namespace

int main() {
  // --- x64 System V (darwin/linux): this=rdi, result=rax ------------------
  // mov rax, [rdi+disp8]; ret   -> 48 8b 47 <d8> c3
  ExpectOk("sysv mov-rax-[rdi]-disp8-ret",
           MatchX64SysVFieldGetter(Pad16({0x48, 0x8b, 0x47, 0x40, 0xc3}).data(),
                                   "test"),
           0x40, GetterCallMode::kDirectReturn);

  // mov rax, [rdi+disp32]; ret  -> 48 8b 87 <d32> c3
  {
    std::vector<uint8_t> code = {0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x1b8);
    code.push_back(0xc3);
    ExpectOk("sysv mov-rax-[rdi]-disp32-ret",
             MatchX64SysVFieldGetter(Pad16(code).data(), "test"),
             0x1b8, GetterCallMode::kDirectReturn);
  }

  // Frame-pointer prologue: push rbp; mov rbp,rsp; mov rax,[rdi+disp32];
  // pop rbp; ret -> 55 48 89 e5 48 8b 87 <d32> 5d c3
  {
    std::vector<uint8_t> code = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x2000);
    code.push_back(0x5d);
    code.push_back(0xc3);
    ExpectOk("sysv framed mov-rax-[rdi]-disp32-ret",
             MatchX64SysVFieldGetter(Pad16(code).data(), "test"),
             0x2000, GetterCallMode::kDirectReturn);
  }

  // Frame-pointer prologue with disp8:
  // 55 48 89 e5 48 8b 47 <d8> 5d c3
  ExpectOk("sysv framed mov-rax-[rdi]-disp8-ret",
           MatchX64SysVFieldGetter(
               Pad16({0x55, 0x48, 0x89, 0xe5, 0x48, 0x8b, 0x47, 0x30, 0x5d,
                      0xc3})
                   .data(),
               "test"),
           0x30, GetterCallMode::kDirectReturn);

  // endbr64 landing pad then disp8 load.
  ExpectOk("sysv endbr64 mov-rax-[rdi]-disp8-ret",
           MatchX64SysVFieldGetter(
               Pad16({0xf3, 0x0f, 0x1e, 0xfa, 0x48, 0x8b, 0x47, 0x18, 0xc3})
                   .data(),
               "test"),
           0x18, GetterCallMode::kDirectReturn);

  // --- x64 Win64: this=rcx, result=rax; sret path uses rdx --------------
  // mov rax, [rcx+disp8]; ret   -> 48 8b 41 <d8> c3
  ExpectOk("win64 mov-rax-[rcx]-disp8-ret",
           MatchX64Win64FieldGetter(Pad16({0x48, 0x8b, 0x41, 0x28, 0xc3}).data(),
                                    "test"),
           0x28, GetterCallMode::kDirectReturn);

  // mov rax, [rcx+disp32]; ret  -> 48 8b 81 <d32> c3
  {
    std::vector<uint8_t> code = {0x48, 0x8b, 0x81};
    PushDisp32(&code, 0x1c0);
    code.push_back(0xc3);
    ExpectOk("win64 mov-rax-[rcx]-disp32-ret",
             MatchX64Win64FieldGetter(Pad16(code).data(), "test"),
             0x1c0, GetterCallMode::kDirectReturn);
  }

  // sret disp32: mov rax,[rcx+disp32]; mov [rdx],rax; mov rax,rdx; ret
  // 48 8b 81 <d32> 48 89 02 48 8b c2 c3
  {
    std::vector<uint8_t> code = {0x48, 0x8b, 0x81};
    PushDisp32(&code, 0x1c0);
    for (uint8_t b : {0x48, 0x89, 0x02, 0x48, 0x8b, 0xc2, 0xc3})
      code.push_back(b);
    ExpectOk("win64 sret mov-[rdx]-[rcx]-disp32-ret",
             MatchX64Win64FieldGetter(Pad16(code).data(), "test"),
             0x1c0, GetterCallMode::kSret);
  }

  // sret disp8: mov rax,[rcx+disp8]; mov [rdx],rax; mov rax,rdx; ret
  // 48 8b 41 <d8> 48 89 02 48 8b c2 c3
  ExpectOk("win64 sret mov-[rdx]-[rcx]-disp8-ret",
           MatchX64Win64FieldGetter(
               Pad16({0x48, 0x8b, 0x41, 0x38, 0x48, 0x89, 0x02, 0x48, 0x8b,
                      0xc2, 0xc3})
                   .data(),
               "test"),
           0x38, GetterCallMode::kSret);

  // sret variant seen on some builds: mov rax,rdx; mov rcx,[rcx+disp32];
  // mov [rdx],rcx; ret -> 48 89 d0 48 8b 89 <d32> 48 89 0a c3
  {
    std::vector<uint8_t> code = {0x48, 0x89, 0xd0, 0x48, 0x8b, 0x89};
    PushDisp32(&code, 0x1c8);
    for (uint8_t b : {0x48, 0x89, 0x0a, 0xc3}) code.push_back(b);
    ExpectOk("win64 sret mov-rax-rdx-load-rcx-disp32",
             MatchX64Win64FieldGetter(Pad16(code).data(), "test"),
             0x1c8, GetterCallMode::kSret);
  }

  // Same variant with a disp8 load:
  // mov rax,rdx; mov rcx,[rcx+disp8]; mov [rdx],rcx; ret
  // 48 89 d0 48 8b 49 <d8> 48 89 0a c3
  ExpectOk("win64 sret mov-rax-rdx-load-rcx-disp8",
           MatchX64Win64FieldGetter(
               Pad16({0x48, 0x89, 0xd0, 0x48, 0x8b, 0x49, 0x50, 0x48, 0x89,
                      0x0a, 0xc3})
                   .data(),
               "test"),
           0x50, GetterCallMode::kSret);

  // --- arm64: optional bti c; ldr x0,[x0,#imm]; ret ----------------------
  // ldr x0,[x0,#0x1b8] => imm12 = 0x1b8/8 = 0x37 -> 00 dc 40 f9, ret d65f03c0
  ExpectOk("arm64 ldr-x0-[x0]-ret",
           MatchArm64FieldGetter(
               Arm64Words({0xf9400000u | (0x37u << 10), 0xd65f03c0u}).data(),
               "test"),
           0x1b8, GetterCallMode::kDirectReturn);

  // bti c; ldr x0,[x0,#0x40]; ret
  ExpectOk("arm64 bti-c-ldr-x0-[x0]-ret",
           MatchArm64FieldGetter(
               Arm64Words({0xd503245fu, 0xf9400000u | (0x08u << 10),
                           0xd65f03c0u})
                   .data(),
               "test"),
           0x40, GetterCallMode::kDirectReturn);

  // --- win32-arm64 (MSVC): sret via x1, or direct via x0 -----------------
  // Direct form is identical to the Itanium getter: ldr x0,[x0,#0x40]; ret
  ExpectOk("win32-arm64 direct ldr-x0-[x0]-ret",
           MatchArm64Win64FieldGetter(
               Arm64Words({0xf9400000u | (0x08u << 10), 0xd65f03c0u}).data(),
               "test"),
           0x40, GetterCallMode::kDirectReturn);

  // Sret form: ldr x8,[x0,#0x1b8]; str x8,[x1]; mov x0,x1; ret
  //   ldr x8,[x0,#0x1b8] => imm12=0x37, Rt=8 -> f9 40 dc 08
  //   str x8,[x1]        => Rn=x1, Rt=8      -> f9 00 00 28
  //   mov x0,x1                              -> aa 01 03 e0
  ExpectOk("win32-arm64 sret ldr-str-mov-ret",
           MatchArm64Win64FieldGetter(
               Arm64Words({0xf9400000u | (0x37u << 10) | 0x08u,
                           0xf9000020u | 0x08u, 0xaa0103e0u, 0xd65f03c0u})
                   .data(),
               "test"),
           0x1b8, GetterCallMode::kSret);

  // Sret form as emitted by MSVC on the CI runner, with the return-pointer move
  // ahead of the store: ldr x8,[x0,#0x168]; mov x0,x1; str x8,[x1]; ret
  //   ldr x8,[x0,#0x168] => imm12=0x2d, Rt=8 -> f9 40 b4 08
  ExpectOk("win32-arm64 sret ldr-mov-str-ret",
           MatchArm64Win64FieldGetter(
               Arm64Words({0xf9400000u | (0x2du << 10) | 0x08u, 0xaa0103e0u,
                           0xf9000020u | 0x08u, 0xd65f03c0u})
                   .data(),
               "test"),
           0x168, GetterCallMode::kSret);

  // Sret form without the trailing mov: ldr x8,[x0,#0x40]; str x8,[x1]; ret
  ExpectOk("win32-arm64 sret ldr-str-ret",
           MatchArm64Win64FieldGetter(
               Arm64Words({0xf9400000u | (0x08u << 10) | 0x08u,
                           0xf9000020u | 0x08u, 0xd65f03c0u})
                   .data(),
               "test"),
           0x40, GetterCallMode::kSret);

  // Sret store from a different register than the load: rejected.
  ExpectReject("win32-arm64 sret store-src-mismatch",
               MatchArm64Win64FieldGetter(
                   Arm64Words({0xf9400000u | (0x08u << 10) | 0x09u,
                               0xf9000020u | 0x08u, 0xd65f03c0u})
                       .data(),
                   "test"));

  // --- win32-x86 (MSVC __thiscall): this=ecx, sret via ret imm16 ---------
  // Direct: mov eax,[ecx+disp8]; ret  -> 8b 41 <d8> c3
  ExpectOk("win32-x86 direct mov-eax-[ecx]-disp8-ret",
           MatchX86ThiscallFieldGetter(
               Pad16({0x8b, 0x41, 0x28, 0xc3}).data(), "test"),
           0x28, GetterCallMode::kDirectReturn);

  // Direct disp32: mov eax,[ecx+disp32]; ret  -> 8b 81 <d32> c3
  {
    std::vector<uint8_t> code = {0x8b, 0x81};
    PushDisp32(&code, 0x1c0);
    code.push_back(0xc3);
    ExpectOk("win32-x86 direct mov-eax-[ecx]-disp32-ret",
             MatchX86ThiscallFieldGetter(Pad16(code).data(), "test"),
             0x1c0, GetterCallMode::kDirectReturn);
  }

  // Sret: mov ecx,[ecx+disp8]; mov [sret],ecx; mov eax,[esp+4]; ret 4
  //   mov eax,[esp+4] -> 8b 44 24 04 (SIB, base=esp, ignored)
  //   ret 4           -> c2 04 00
  ExpectOk("win32-x86 sret ret-imm16",
           MatchX86ThiscallFieldGetter(
               Pad16({0x8b, 0x49, 0x30, 0x8b, 0x44, 0x24, 0x04, 0x89, 0x08,
                      0xc2, 0x04, 0x00})
                   .data(),
               "test"),
           0x30, GetterCallMode::kSret);

  // Framed sret: push ebp; mov ebp,esp; mov eax,[ecx+disp32]; pop ebp; ret 4
  {
    std::vector<uint8_t> code = {0x55, 0x8b, 0xec, 0x8b, 0x81};
    PushDisp32(&code, 0x1c8);
    for (uint8_t b : {0x5d, 0xc2, 0x04, 0x00}) code.push_back(b);
    ExpectOk("win32-x86 framed sret ret-imm16",
             MatchX86ThiscallFieldGetter(Pad16(code).data(), "test"),
             0x1c8, GetterCallMode::kSret);
  }

  // Direct getter that returns via a register other than eax: rejected.
  // mov ecx,[ecx+disp8]; ret -> 8b 49 20 c3
  ExpectReject("win32-x86 direct non-eax return",
               MatchX86ThiscallFieldGetter(
                   Pad16({0x8b, 0x49, 0x20, 0xc3}).data(), "test"));

  // Zero offset is implausible: mov eax,[ecx]; ret -> 8b 01 c3
  ExpectReject("win32-x86 zero-offset",
               MatchX86ThiscallFieldGetter(
                   Pad16({0x8b, 0x01, 0xc3}).data(), "test"));

  // --- rejections --------------------------------------------------------
  // Reads the wrong base register (rsi=6 instead of rdi): 48 8b 46 08 c3
  ExpectReject("sysv wrong-base-register",
               MatchX64SysVFieldGetter(
                   Pad16({0x48, 0x8b, 0x46, 0x08, 0xc3}).data(), "test"));

  // Offset zero is implausible: mov rax,[rdi]; ret -> 48 8b 07 c3
  ExpectReject("sysv zero-offset",
               MatchX64SysVFieldGetter(Pad16({0x48, 0x8b, 0x07, 0xc3}).data(),
                                       "test"));

  // No ret / all zero.
  ExpectReject("sysv all-zero", MatchX64SysVFieldGetter(Pad16({}).data(),
                                                        "test"));

  // arm64 missing ret.
  ExpectReject("arm64 missing-ret",
               MatchArm64FieldGetter(
                   Arm64Words({0xf9400000u | (0x37u << 10), 0x0}).data(),
                   "test"));

  if (g_failures == 0) {
    std::printf("\nAll getter-decoder cases passed.\n");
    return 0;
  }
  std::printf("\n%d getter-decoder case(s) FAILED.\n", g_failures);
  return 1;
}
