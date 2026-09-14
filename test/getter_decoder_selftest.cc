// Standalone verification for the x64/arm64/x86 getter decoders. Feeds each decoder
// the exact machine-code shapes the per-platform parsers recognize and asserts
// the decoded offset and call mode. The decoders are host-architecture
// independent, so this runs on any build host regardless of the target it
// decodes for. Compiled and run by scripts/test-getter-decoder.ts.
#include "runtime_probe/getter_decoder.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using esplus::node::require_builtin::GetterCallMode;
using esplus::node::require_builtin::GetterPattern;
using esplus::node::require_builtin::MatchArm64AapcsFieldGetter;
using esplus::node::require_builtin::MatchArm64FieldGetter;
using esplus::node::require_builtin::MatchArm64Win64FieldGetter;
using esplus::node::require_builtin::MatchX64SysVFieldGetter;
using esplus::node::require_builtin::MatchX64Win64FieldGetter;
using esplus::node::require_builtin::MatchX86ThiscallFieldGetter;
using esplus::node::require_builtin::Result;

namespace {

using esplus::node::require_builtin::kGetterCodeWindowBytes;
using esplus::node::require_builtin::kHardenedGetterCodeWindowBytes;

#include "getter_fixtures.inc"

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

// Asserts the exact diagnostic pattern text, not just the offset. This is what
// pins the guarantee that routing a platform through MatchArm64AapcsFieldGetter
// reports identically to MatchArm64FieldGetter for the shapes the latter already
// accepts.
void ExpectPattern(const char* name,
                   Result<GetterPattern> result,
                   size_t expected_offset,
                   const char* expected_pattern) {
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
  if (pattern.pattern != expected_pattern) {
    std::printf("FAIL %-52s pattern=\"%s\" expected=\"%s\"\n", name,
                pattern.pattern.c_str(), expected_pattern);
    g_failures++;
    return;
  }
  std::printf("ok   %-52s offset=0x%zx [%s]\n", name, pattern.offset,
              pattern.pattern.c_str());
}

// Checks the decoded offset and reports which decode stage resolved the shape.
// Used by the archived-fixture loop, where the pattern text varies with each
// shape's hardening but the offset must not.
void ExpectOffsetAtStage(const char* name,
                         Result<GetterPattern> result,
                         size_t expected_offset,
                         int stage) {
  if (!result.ok()) {
    std::printf("FAIL %-52s parse rejected: %s\n", name,
                result.status().message().c_str());
    g_failures++;
    return;
  }
  if (result.value().offset != expected_offset) {
    std::printf("FAIL %-52s offset=0x%zx expected=0x%zx\n", name,
                result.value().offset, expected_offset);
    g_failures++;
    return;
  }
  std::printf("ok   %-52s stage=%d offset=0x%zx [%s]\n", name, stage,
              result.value().offset, result.value().pattern.c_str());
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

// Hardened x86-64 bodies exceed the 16-byte window, so they are fed through the
// wider one that MatchX64SysVFieldGetter accepts on request.
std::vector<uint8_t> Pad32(std::vector<uint8_t> bytes) {
  bytes.resize(32, 0x00);
  return bytes;
}

// Little-endian disp32 helper.
void PushDisp32(std::vector<uint8_t>* out, uint32_t value) {
  out->push_back(value & 0xff);
  out->push_back((value >> 8) & 0xff);
  out->push_back((value >> 16) & 0xff);
  out->push_back((value >> 24) & 0xff);
}

// `words` defaults to the four-instruction fast-path window; hardened shapes are
// longer and pass the wider window MatchArm64AapcsFieldGetter reads.
std::vector<uint32_t> Arm64Words(std::vector<uint32_t> words_in,
                                 size_t words = 4) {
  words_in.resize(words, 0);
  return words_in;
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

  // Electron 43 darwin-x64, read from Electron Framework:
  // push rbp; mov rbp,rsp; mov rax,[rdi+0x1b8]; pop rbp; ret
  {
    std::vector<uint8_t> code = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x1b8);
    code.push_back(0x5d);
    code.push_back(0xc3);
    ExpectOk("fixture x64/electron43-darwin",
             MatchX64SysVFieldGetter(Pad16(code).data(), "fx"),
             0x1b8, GetterCallMode::kDirectReturn);
  }

  // Electron 44/45 darwin-x64, read from Electron Framework:
  // push rbp; mov rbp,rsp; mov rax,[rdi+0x1c0]; pop rbp; ret
  {
    std::vector<uint8_t> code = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x1c0);
    code.push_back(0x5d);
    code.push_back(0xc3);
    ExpectOk("fixture x64/electron44-45-darwin",
             MatchX64SysVFieldGetter(Pad16(code).data(), "fx"),
             0x1c0, GetterCallMode::kDirectReturn);
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

  // --- x64 System V hardened: needs the wider window ----------------------
  // The shape Fedora's x86_64 libnode actually has today — endbr64 then a disp32
  // load — is 12 bytes and fits the standard window, which is why hardened
  // x86-64 was never broken. Confirm that explicitly.
  {
    std::vector<uint8_t> code = {0xf3, 0x0f, 0x1e, 0xfa, 0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x200);
    code.push_back(0xc3);
    ExpectOk("sysv endbr64 disp32 fits standard window",
             MatchX64SysVFieldGetter(Pad16(code).data(), "test"),
             0x200, GetterCallMode::kDirectReturn);
  }

  // But endbr64 *and* a frame pointer around a disp32 load is 17 bytes, so `ret`
  // falls outside the standard window and the getter is rejected there — the
  // latent break that a compiler-flag change would expose.
  {
    std::vector<uint8_t> code = {0xf3, 0x0f, 0x1e, 0xfa, 0x55, 0x48, 0x89,
                                 0xe5, 0x48, 0x8b, 0x87};
    PushDisp32(&code, 0x200);
    code.push_back(0x5d);
    code.push_back(0xc3);
    ExpectReject("sysv endbr64+framed disp32 rejected in standard window",
                 MatchX64SysVFieldGetter(Pad32(code).data(), "test"));
    // Same bytes, wider window: accepted, and the pattern text is unchanged from
    // what any other framed disp32 body reports.
    ExpectPattern("sysv endbr64+framed disp32 in wide window",
                  MatchX64SysVFieldGetter(Pad32(code).data(), "test", 32),
                  0x200, "test mov-rax-[this-rdi] disp32");
  }

  // Widening the window must not start accepting non-getters: an unrecognized
  // instruction still fails closed with the extra bytes available.
  ExpectReject("sysv wide window rejects unknown insn",
               MatchX64SysVFieldGetter(
                   Pad32({0xf3, 0x0f, 0x1e, 0xfa, 0x48, 0x01, 0xc7, 0x48, 0x8b,
                          0x47, 0x40, 0xc3})
                       .data(),
                   "test", 32));

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

  // Electron 44 win32-x64, read from electron.exe:
  // mov rax,rdx; mov rcx,[rcx+0x1c0]; mov [rdx],rcx; ret
  {
    std::vector<uint8_t> code = {0x48, 0x89, 0xd0, 0x48, 0x8b, 0x89};
    PushDisp32(&code, 0x1c0);
    for (uint8_t b : {0x48, 0x89, 0x0a, 0xc3}) code.push_back(b);
    ExpectOk("fixture x64/electron44-win32",
             MatchX64Win64FieldGetter(Pad16(code).data(), "fx"),
             0x1c0, GetterCallMode::kSret);
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

  // Electron 43 darwin-arm64, read from Electron Framework:
  // ldr x0,[x0,#0x1b8]; ret
  ExpectOk("fixture arm64/electron43-darwin",
           MatchArm64FieldGetter(
               Arm64Words({0xf940dc00, 0xd65f03c0, 0xf940e000, 0xd65f03c0})
                   .data(),
               "fx"),
           0x1b8, GetterCallMode::kDirectReturn);

  // Electron 44/45 darwin-arm64, read from Electron Framework:
  // ldr x0,[x0,#0x1c0]; ret
  ExpectOk("fixture arm64/electron44-45-darwin",
           MatchArm64FieldGetter(
               Arm64Words({0xf940e000, 0xd65f03c0, 0xf9412000, 0xd65f03c0})
                   .data(),
               "fx"),
           0x1c0, GetterCallMode::kDirectReturn);

  // Electron 43 linux-arm64 adds BTI to the same 0x1b8 field getter.
  ExpectOk("fixture arm64/electron43-linux",
           MatchArm64AapcsFieldGetter(
               Arm64Words({0xd503245f, 0xf940dc00, 0xd65f03c0}).data(),
               "fx", 16),
           0x1b8, GetterCallMode::kDirectReturn);

  // Electron 44/45 linux-arm64 use BTI and the 0x1c0 Realm field.
  ExpectOk("fixture arm64/electron44-45-linux",
           MatchArm64AapcsFieldGetter(
               Arm64Words({0xd503245f, 0xf940e000, 0xd65f03c0}).data(),
               "fx", 16),
           0x1c0, GetterCallMode::kDirectReturn);

  // --- arm64 AAPCS walker (linux): tolerates hardened prologue/epilogue ---
  // The real shape Fedora 44 emits for both nodejs22 (libnode.so.127) and
  // nodejs24 (libnode.so.137) on aarch64, verified by disassembling the packaged
  // libnode: paciasp; stp x29,x30,[sp,#-16]!; mov x29,sp; ldr x0,[x0,#0x200];
  // ldp x29,x30,[sp],#16; autiasp; ret. Seven instructions, so it needs the wide
  // window; imm12 = 0x200/8 = 0x40.
  ExpectPattern("arm64 aapcs fedora pac-frame",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503233fu, 0xa9bf7bfdu, 0x910003fdu,
                                0xf9400000u | (0x40u << 10), 0xa8c17bfdu,
                                0xd50323bfu, 0xd65f03c0u},
                               8)
                        .data(),
                    "linux-glibc-arm64", 32),
                0x200, "linux-glibc-arm64 pac-frame-ldr-x0-[this-imm]-ret");

  // Unhardened shapes must report byte-identically to MatchArm64FieldGetter, so
  // moving linux-arm64 onto the walker cannot change existing diagnostics.
  ExpectPattern("arm64 aapcs plain text matches fast path",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xf9400000u | (0x37u << 10), 0xd65f03c0u})
                        .data(),
                    "test", 16),
                0x1b8, "test ldr-x0-[this-imm]-ret");

  ExpectPattern("arm64 aapcs bti text matches fast path",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503245fu, 0xf9400000u | (0x08u << 10),
                                0xd65f03c0u})
                        .data(),
                    "test", 16),
                0x40, "test bti-c-ldr-x0-[this-imm]-ret");

  // b-key pointer authentication (pacibsp/autibsp) is the same shape.
  ExpectPattern("arm64 aapcs b-key pac-frame",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503237fu, 0xa9bf7bfdu, 0x910003fdu,
                                0xf9400000u | (0x08u << 10), 0xa8c17bfdu,
                                0xd50323ffu, 0xd65f03c0u},
                               8)
                        .data(),
                    "test", 32),
                0x40, "test pac-frame-ldr-x0-[this-imm]-ret");

  // A hardened body does not fit the narrow window, so a caller that can only
  // guarantee 16 readable bytes must fail closed rather than read past it.
  ExpectReject("arm64 aapcs hardened rejected in narrow window",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0xa9bf7bfdu, 0x910003fdu,
                               0xf9400000u | (0x40u << 10), 0xa8c17bfdu,
                               0xd50323bfu, 0xd65f03c0u},
                              8)
                       .data(),
                   "test", 16));

  // An unrecognized instruction between the frame setup and the load must fail
  // closed instead of being skipped: `add x0,x0,#8` would rebase `this`.
  ExpectReject("arm64 aapcs unknown-insn",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0x91002000u,
                               0xf9400000u | (0x40u << 10), 0xd65f03c0u},
                              8)
                       .data(),
                   "test", 32));

  // Two field loads mean this is not a plain accessor; the offset would be a
  // guess between them.
  ExpectReject("arm64 aapcs two-loads",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0xf9400000u | (0x40u << 10),
                               0xf9400000u | (0x41u << 10), 0xd65f03c0u},
                              8)
                       .data(),
                   "test", 32));

  // Frame teardown but no load at all.
  ExpectReject("arm64 aapcs no-load",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0xa9bf7bfdu, 0x910003fdu,
                               0xa8c17bfdu, 0xd50323bfu, 0xd65f03c0u},
                              8)
                       .data(),
                   "test", 32));

  // Runs off the end of the window without ever reaching ret.
  ExpectReject("arm64 aapcs missing-ret",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0xf9400000u | (0x40u << 10)}, 8)
                       .data(),
                   "test", 32));

  // ldr x0,[x0,#0] is a vptr read, not a field read: offset 0 is implausible.
  ExpectReject("arm64 aapcs zero-offset",
               MatchArm64AapcsFieldGetter(
                   Arm64Words({0xd503233fu, 0xf9400000u, 0xd65f03c0u}, 8)
                       .data(),
                   "test", 32));

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

  // Electron 45 win32-arm64, read from electron.exe:
  // ldr x8,[x0,#0x1c0]; mov x0,x1; str x8,[x1]; ret
  ExpectOk("fixture arm64/electron45-win32",
           MatchArm64Win64FieldGetter(
               Arm64Words({0xf940e008, 0xaa0103e0, 0xf9000028, 0xd65f03c0})
                   .data(),
               "fx"),
           0x1c0, GetterCallMode::kSret);

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
               Pad32({0x8b, 0x41, 0x28, 0xc3}).data(), "test"),
           0x28, GetterCallMode::kDirectReturn);

  // Direct disp32: mov eax,[ecx+disp32]; ret  -> 8b 81 <d32> c3
  {
    std::vector<uint8_t> code = {0x8b, 0x81};
    PushDisp32(&code, 0x1c0);
    code.push_back(0xc3);
    ExpectOk("win32-x86 direct mov-eax-[ecx]-disp32-ret",
             MatchX86ThiscallFieldGetter(Pad32(code).data(), "test"),
             0x1c0, GetterCallMode::kDirectReturn);
  }

  // Sret: mov ecx,[ecx+disp8]; mov [sret],ecx; mov eax,[esp+4]; ret 4
  //   mov eax,[esp+4] -> 8b 44 24 04 (SIB, base=esp, ignored)
  //   ret 4           -> c2 04 00
  ExpectOk("win32-x86 sret ret-imm16",
           MatchX86ThiscallFieldGetter(
               Pad32({0x8b, 0x49, 0x30, 0x8b, 0x44, 0x24, 0x04, 0x89, 0x08,
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
             MatchX86ThiscallFieldGetter(Pad32(code).data(), "test"),
             0x1c8, GetterCallMode::kSret);
  }

  // Electron 43 win32-ia32, read from electron.exe:
  // push ebp; mov ebp,esp; mov eax,[ebp+8]; mov ecx,[ecx+0xdc];
  // mov [eax],ecx; pop ebp; ret 4
  ExpectOk("fixture ia32/electron43-win32",
           MatchX86ThiscallFieldGetter(
               Pad32({0x55, 0x89, 0xe5, 0x8b, 0x45, 0x08, 0x8b, 0x89, 0xdc,
                      0x00, 0x00, 0x00, 0x89, 0x08, 0x5d, 0xc2, 0x04, 0x00})
                   .data(),
               "fx"),
           0xdc, GetterCallMode::kSret);

  // Direct getter that returns via a register other than eax: rejected.
  // mov ecx,[ecx+disp8]; ret -> 8b 49 20 c3
  ExpectReject("win32-x86 direct non-eax return",
               MatchX86ThiscallFieldGetter(
                   Pad32({0x8b, 0x49, 0x20, 0xc3}).data(), "test"));

  // Zero offset is implausible: mov eax,[ecx]; ret -> 8b 01 c3
  ExpectReject("win32-x86 zero-offset",
               MatchX86ThiscallFieldGetter(
                   Pad32({0x8b, 0x01, 0xc3}).data(), "test"));

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

  // Frame size must not be baked into the matcher. A 32-byte frame
  // (stp x29,x30,[sp,#-32]! / ldp x29,x30,[sp],#32) is the same shape as the
  // 16-byte one Fedora happens to emit, and pinning the size would reject it.
  ExpectPattern("arm64 aapcs pac-frame with 32-byte frame",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503233fu, 0xa9be7bfdu, 0x910003fdu,
                                0xf9400000u | (0x40u << 10), 0xa8c27bfdu,
                                0xd50323bfu, 0xd65f03c0u},
                               8)
                        .data(),
                    "test", 32),
                0x200, "test pac-frame-ldr-x0-[this-imm]-ret");

  // retaa / retab terminate the body on clang-built hardened binaries, where GCC
  // would emit a separate autiasp; ret.
  ExpectPattern("arm64 aapcs paciasp-ldr-retaa",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503233fu, 0xf9400000u | (0x40u << 10),
                                0xd65f0bffu},
                               8)
                        .data(),
                    "test", 32),
                0x200, "test pac-ldr-x0-[this-imm]-ret");

  ExpectPattern("arm64 aapcs pacibsp-ldr-retab",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503237fu, 0xf9400000u | (0x40u << 10),
                                0xd65f0fffu},
                               8)
                        .data(),
                    "test", 32),
                0x200, "test pac-ldr-x0-[this-imm]-ret");

  // A frame record on registers other than x29/x30, or based on something other
  // than sp, is not a frame record we understand and must still fail closed.
  ExpectReject("arm64 aapcs stp-wrong-registers",
               MatchArm64AapcsFieldGetter(
                   // stp x19, x20, [sp, #-16]!
                   Arm64Words({0xd503233fu, 0xa9bf53f3u,
                               0xf9400000u | (0x40u << 10), 0xd65f03c0u},
                              8)
                       .data(),
                   "test", 32));

  // Shape and offset are independent inputs to the decoder: the shape decides
  // whether the body is accepted, the `ldr` immediate alone decides the offset,
  // which is never hard-coded anywhere. This case combines the two dimensions
  // that were each verified against a real binary — Fedora's hardened aarch64
  // body, and the 0x208 field offset official Node 26 moved to (Node 24 used
  // 0x1f8) — which is what a Fedora-packaged Node 26 would look like before
  // Fedora ships one to read directly.
  ExpectPattern("arm64 aapcs fedora shape with node26 offset",
                MatchArm64AapcsFieldGetter(
                    Arm64Words({0xd503233fu, 0xa9bf7bfdu, 0x910003fdu,
                                0xf9400000u | (0x41u << 10), 0xa8c17bfdu,
                                0xd50323bfu, 0xd65f03c0u},
                               8)
                        .data(),
                    "test", 32),
                0x208, "test pac-frame-ldr-x0-[this-imm]-ret");

  // --- walker is a strict superset of the fixed-position matcher ------------
  // MatchArm64FieldGetter's entire input space is enumerable: an optional `bti c`
  // followed by `ldr x0,[x0,#imm12*8]` and `ret`, so 2 x 4096 shapes. Check every
  // one of them rather than asserting the containment in a comment. Wherever the
  // fixed-position matcher accepts, the walker must accept with an identical
  // offset and an identical pattern string; wherever it rejects, the walker must
  // not silently invent a different offset.
  {
    size_t accepted = 0;
    size_t mismatches = 0;
    for (int bti = 0; bti < 2; bti++) {
      for (uint32_t imm12 = 0; imm12 < 4096; imm12++) {
        std::vector<uint32_t> body;
        if (bti) body.push_back(0xd503245fu);
        body.push_back(0xf9400000u | (imm12 << 10));
        body.push_back(0xd65f03c0u);
        std::vector<uint32_t> words = Arm64Words(body);

        Result<GetterPattern> fixed =
            MatchArm64FieldGetter(words.data(), "t");
        Result<GetterPattern> walk = MatchArm64AapcsFieldGetter(
            words.data(), "t", kGetterCodeWindowBytes);

        if (!fixed.ok()) {
          // The only rejections here are implausible offsets, which the walker
          // must reject for the same reason.
          if (walk.ok()) mismatches++;
          continue;
        }
        accepted++;
        if (!walk.ok() || walk.value().offset != fixed.value().offset ||
            walk.value().pattern != fixed.value().pattern) {
          mismatches++;
        }
      }
    }
    if (mismatches != 0) {
      std::printf("FAIL %-52s %zu of 8192 shapes disagree\n",
                  "arm64 walker superset of fixed-position", mismatches);
      g_failures++;
    } else {
      std::printf(
          "ok   %-52s 8192 shapes, %zu accepted, all identical\n",
          "arm64 walker superset of fixed-position", accepted);
    }
  }

  // --- archived real machine code -----------------------------------------
  // Every fixture in test/getter_fixtures.inc, which holds bytes emitted by real
  // toolchains across an optimization/hardening matrix plus bytes read out of
  // shipped Node binaries. Hand-written cases above can only cover shapes we
  // anticipated; these cover what compilers and distributions actually produce.
  // The expected offsets were derived by an independent decoder in the generator
  // script, so agreement here is a genuine cross-check.
  //
  // Each fixture runs through the same two-stage sequence the linux_glibc_*
  // parsers use — standard window first, wider window only on failure — so this
  // exercises the production decode path rather than a single matcher. The stage
  // that resolved each shape is reported, which is what documents that only
  // genuinely long bodies (Fedora's 28-byte frame record) need the wider window.
  for (const GetterFixture& fx : kGetterFixtures) {
    // Fixtures are byte buffers of their own exact length. Copy into a padded
    // window so a matcher can read a full window without running off the end,
    // mirroring what the real probe guarantees via IsPlatformReadableRange.
    unsigned char window[64] = {0};
    const size_t copy = fx.size < sizeof(window) ? fx.size : sizeof(window);
    std::memcpy(window, fx.bytes, copy);

    const bool is_arm64 = std::strcmp(fx.arch, "arm64") == 0;
    auto match = [&](size_t window_bytes) {
      return is_arm64 ? MatchArm64AapcsFieldGetter(window, "fx", window_bytes)
                      : MatchX64SysVFieldGetter(window, "fx", window_bytes);
    };

    int stage = 1;
    Result<GetterPattern> result = match(kGetterCodeWindowBytes);
    if (!result.ok()) {
      stage = 2;
      result = match(kHardenedGetterCodeWindowBytes);
    }

    char name[192];
    std::snprintf(name, sizeof(name), "fixture %s/%s %s", fx.arch, fx.source,
                  fx.config);

    if (fx.decodable) {
      ExpectOffsetAtStage(name, std::move(result), fx.expected_offset, stage);
    } else {
      // Unoptimized builds spill `this` through the stack before loading the
      // field, so the body is not a plain accessor. Shipped Node is never built
      // that way, and accepting stack traffic would widen the accepted set far
      // more than it is worth — the rejection is deliberate.
      ExpectReject(name, std::move(result));
    }
  }

  if (g_failures == 0) {
    std::printf("\nAll getter-decoder cases passed.\n");
    return 0;
  }
  std::printf("\n%d getter-decoder case(s) FAILED.\n", g_failures);
  return 1;
}
