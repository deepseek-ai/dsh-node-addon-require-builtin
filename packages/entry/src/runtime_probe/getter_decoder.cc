#include "getter_decoder.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace internal_require {
namespace {

// A field getter can only reference the object a handful of words into itself.
constexpr size_t kMaxReasonableRealmOffset = 0x4000;

// Longest getter body we recognize (frame-pointer prologue + disp32 load +
// struct-return epilogue + ret) fits in 14 bytes. Keep the copied window at 16
// to match the historical read footprint: the Windows vtable scanner parses
// arbitrary executable candidates, so this must not read further than the
// original fixed-16-byte probe did. The decoders fail closed if a body needs
// more than the window holds.
constexpr size_t kX64CodeWindow = 16;
constexpr size_t kArm64WordWindow = 3;

bool IsPlausibleOffset(size_t offset) {
  return offset != 0 &&
      offset <= kMaxReasonableRealmOffset &&
      (offset % alignof(void*)) == 0;
}

Result<GetterPattern> Failure(const char* message) {
  return Result<GetterPattern>::Failure(
      Status::Failure(ProbeStatus::kUnsupportedNoGetter, message));
}

// --- x86-64 ---------------------------------------------------------------
//
// We do not need a general disassembler, only enough to walk the tiny getter
// body one instruction at a time and recognize the forms a field accessor can
// take. Any instruction outside that set makes the whole match fail closed.

// Register numbers use the low-3-bit x86 encoding (RAX=0, RCX=1, RDX=2, RDI=7).
constexpr uint8_t kRegRax = 0;
constexpr uint8_t kRegRcx = 1;
constexpr uint8_t kRegRdx = 2;
constexpr uint8_t kRegRdi = 7;

enum class X64Op {
  kEndbr64,     // f3 0f 1e fa            CET landing pad
  kPushRbp,     // 55
  kPopRbp,      // 5d
  kRet,         // c3
  kMovRegMem,   // REX.W 8b /r            reg <- [base + disp]
  kMovMemReg,   // REX.W 89 /r            [base + disp] <- reg
  kMovRegReg,   // REX.W 89|8b /r, mod=11 reg <-> reg
  kUnknown,
};

struct X64Insn {
  X64Op op = X64Op::kUnknown;
  uint8_t reg = 0;        // ModRM.reg operand
  uint8_t base = 0;       // ModRM.rm operand (register, or memory base)
  int64_t disp = 0;       // memory displacement
  bool disp32 = false;    // width of the displacement encoding
  size_t length = 0;      // bytes consumed (0 => decode failed)
};

// Decodes one instruction at `p` (with `remaining` readable bytes). Only the
// instruction shapes that appear in field getters are recognized; anything else
// returns kUnknown so the matcher can reject the getter.
X64Insn DecodeX64(const uint8_t* p, size_t remaining) {
  X64Insn insn;
  if (remaining == 0) return insn;

  if (remaining >= 4 && p[0] == 0xf3 && p[1] == 0x0f && p[2] == 0x1e &&
      p[3] == 0xfa) {
    insn.op = X64Op::kEndbr64;
    insn.length = 4;
    return insn;
  }
  if (p[0] == 0x55) {
    insn.op = X64Op::kPushRbp;
    insn.length = 1;
    return insn;
  }
  if (p[0] == 0x5d) {
    insn.op = X64Op::kPopRbp;
    insn.length = 1;
    return insn;
  }
  if (p[0] == 0xc3) {
    insn.op = X64Op::kRet;
    insn.length = 1;
    return insn;
  }

  // The remaining forms are all REX.W (0x48) MOV r/m64 instructions. Restrict
  // to REX.W with no register extensions (0x48); the getters only touch the
  // legacy low-8 registers, so a wider REX means "not a getter we understand".
  if (p[0] != 0x48 || remaining < 3) return insn;
  const uint8_t opcode = p[1];
  if (opcode != 0x89 && opcode != 0x8b) return insn;

  const uint8_t modrm = p[2];
  const uint8_t mod = modrm >> 6;
  const uint8_t reg = (modrm >> 3) & 0x7;
  const uint8_t rm = modrm & 0x7;

  if (mod == 0x3) {
    // Register-direct MOV (e.g. `mov rax, rdx`, `mov rbp, rsp`).
    insn.op = X64Op::kMovRegReg;
    insn.length = 3;
    if (opcode == 0x8b) {       // reg <- rm
      insn.reg = reg;
      insn.base = rm;
    } else {                    // rm <- reg
      insn.reg = rm;
      insn.base = reg;
    }
    return insn;
  }

  // Memory operand. A SIB byte (rm==RSP) or RIP-relative form (mod==0,rm==RBP)
  // never appears in these `[base + disp]` getters, so reject them.
  if (rm == 0x4 || (mod == 0x0 && rm == 0x5)) return insn;

  size_t length = 3;
  int64_t disp = 0;
  bool disp32 = false;
  if (mod == 0x1) {
    if (remaining < length + 1) return insn;
    disp = static_cast<int8_t>(p[3]);
    length += 1;
  } else if (mod == 0x2) {
    if (remaining < length + 4) return insn;
    uint32_t raw = 0;
    std::memcpy(&raw, p + 3, sizeof(raw));
    disp = static_cast<int32_t>(raw);
    disp32 = true;
    length += 4;
  }
  // mod == 0x0 with a general base register: no displacement.

  insn.op = opcode == 0x8b ? X64Op::kMovRegMem : X64Op::kMovMemReg;
  insn.reg = reg;
  insn.base = rm;
  insn.disp = disp;
  insn.disp32 = disp32;
  insn.length = length;
  return insn;
}

struct X64Body {
  bool ok = false;
  bool found_load = false;    // saw `mov <reg>, [this_reg + disp]`
  uint8_t load_dest = 0;      // register the field was loaded into
  int64_t offset = 0;
  bool disp32 = false;
  bool stores_to_sret = false;  // saw `mov [RDX], <reg>` (struct-return path)
};

// Walks the getter body accepting only the recognized instruction forms, and
// records the single load from `this_reg`. Returns ok=false on any unexpected
// instruction or if the body does not terminate with `ret`.
X64Body WalkX64Getter(const uint8_t* code, size_t size, uint8_t this_reg) {
  X64Body body;
  size_t i = 0;
  while (i < size) {
    const X64Insn insn = DecodeX64(code + i, size - i);
    if (insn.length == 0 || insn.op == X64Op::kUnknown) return body;
    i += insn.length;

    switch (insn.op) {
      case X64Op::kEndbr64:
      case X64Op::kPushRbp:
      case X64Op::kPopRbp:
      case X64Op::kMovRegReg:
        break;  // prologue / epilogue / register shuffling: no effect on offset
      case X64Op::kMovRegMem:
        if (insn.base != this_reg) return body;  // reads an unexpected object
        if (body.found_load) return body;        // getters load one field only
        body.found_load = true;
        body.load_dest = insn.reg;
        body.offset = insn.disp;
        body.disp32 = insn.disp32;
        break;
      case X64Op::kMovMemReg:
        if (insn.base == kRegRdx) body.stores_to_sret = true;
        break;
      case X64Op::kRet:
        body.ok = true;
        return body;
      case X64Op::kUnknown:
        return body;
    }
  }
  return body;  // fell off the end without a ret
}

Result<GetterPattern> BuildPattern(std::string_view platform_tag,
                                   const char* shape,
                                   size_t offset,
                                   bool disp32,
                                   GetterCallMode call_mode) {
  if (!IsPlausibleOffset(offset)) {
    return Failure("parsed getter offset is implausible");
  }
  GetterPattern pattern;
  pattern.offset = offset;
  pattern.call_mode = call_mode;
  std::string text(platform_tag);
  text += ' ';
  text += shape;
  text += disp32 ? " disp32" : " disp8";
  pattern.pattern = std::move(text);
  return Result<GetterPattern>::Ok(pattern);
}

// --- AArch64 --------------------------------------------------------------

constexpr uint32_t kArm64BtiC = 0xd503245fu;
constexpr uint32_t kArm64Ret = 0xd65f03c0u;

// Matches `ldr x0, [x0, #imm]` (LDR immediate, unsigned offset, 64-bit) with
// both the source and destination register being x0, and returns the byte
// offset it loads.
bool DecodeLdrX0FromX0(uint32_t insn, size_t* offset) {
  constexpr uint32_t kMask = 0xffc003ffu;     // opcode + Rn(x0) + Rt(x0)
  constexpr uint32_t kPattern = 0xf9400000u;  // ldr x0, [x0, #imm]
  if ((insn & kMask) != kPattern) return false;
  const uint32_t imm12 = (insn >> 10) & 0xfffu;
  *offset = static_cast<size_t>(imm12) * sizeof(uint64_t);
  return true;
}

}  // namespace

Result<GetterPattern> MatchX64SysVFieldGetter(void* getter,
                                              std::string_view platform_tag) {
  std::array<uint8_t, kX64CodeWindow> code{};
  std::memcpy(code.data(), getter, code.size());

  // System V returns the pointer in RAX and passes `this` in RDI.
  const X64Body body = WalkX64Getter(code.data(), code.size(), kRegRdi);
  if (!body.ok || !body.found_load || body.load_dest != kRegRax ||
      body.stores_to_sret) {
    return Failure("x64 sysv getter is not a recognized this->field accessor");
  }
  return BuildPattern(platform_tag, "mov-rax-[this-rdi]", body.offset,
                      body.disp32, GetterCallMode::kDirectReturn);
}

Result<GetterPattern> MatchX64Win64FieldGetter(void* getter,
                                               std::string_view platform_tag) {
  std::array<uint8_t, kX64CodeWindow> code{};
  std::memcpy(code.data(), getter, code.size());

  // Win64 passes `this` in RCX. Small returns come back in RAX; a struct return
  // adds a hidden sret pointer in RDX that the body stores the field through.
  const X64Body body = WalkX64Getter(code.data(), code.size(), kRegRcx);
  if (!body.ok || !body.found_load) {
    return Failure("win32 x64 getter is not a recognized this->field accessor");
  }
  if (body.stores_to_sret) {
    return BuildPattern(platform_tag, "mov-[sret-rdx]-[this-rcx]", body.offset,
                        body.disp32, GetterCallMode::kSret);
  }
  if (body.load_dest != kRegRax) {
    return Failure("win32 x64 direct getter does not return via rax");
  }
  return BuildPattern(platform_tag, "mov-rax-[this-rcx]", body.offset,
                      body.disp32, GetterCallMode::kDirectReturn);
}

Result<GetterPattern> MatchArm64FieldGetter(void* getter,
                                            std::string_view platform_tag) {
  std::array<uint32_t, kArm64WordWindow> code{};
  std::memcpy(code.data(), getter, code.size() * sizeof(code[0]));

  // Optional BTI landing pad on BTI-enabled builds, then the field load, then
  // return. `this` and the result share x0.
  size_t index = 0;
  const bool has_bti = code[index] == kArm64BtiC;
  if (has_bti) index++;

  size_t offset = 0;
  if (!DecodeLdrX0FromX0(code[index], &offset) ||
      code[index + 1] != kArm64Ret) {
    return Failure("arm64 getter is not optional-bti-ldr-x0-this-imm-ret");
  }
  if (!IsPlausibleOffset(offset)) {
    return Failure("arm64 parsed getter offset is implausible");
  }

  GetterPattern pattern;
  pattern.offset = offset;
  std::string text(platform_tag);
  text += has_bti ? " bti-c-ldr-x0-[this-imm]-ret" : " ldr-x0-[this-imm]-ret";
  pattern.pattern = std::move(text);
  return Result<GetterPattern>::Ok(pattern);
}

}  // namespace internal_require
