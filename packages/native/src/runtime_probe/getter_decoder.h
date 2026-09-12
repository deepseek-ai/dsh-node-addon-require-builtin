#pragma once

#include "../runtime_compat.h"

#include <string_view>

namespace esplus::node::require_builtin {

// Number of bytes each matcher copies out of a candidate getter before
// decoding. Callers that hand the matchers an arbitrary code pointer (e.g. the
// Windows vtable scanner) must ensure at least this many bytes are readable.
//
// 16 bytes is exactly four AArch64 instructions and enough x86-64 for the
// longest recognized field-getter body. This is a hard upper bound, not just a
// buffer size: a getter whose body needs more than the window (e.g. an extra
// AArch64 `bti c` landing pad ahead of an sret `ldr; mov; str; ret`, or a
// framed x86-64 accessor) will not reach its `ret` inside the window and is
// rejected — the matchers fail closed rather than guess. That is safe on the
// current Windows x64/arm64 Node binaries, whose per-realm accessors are leaf
// functions with no BTI pad and no stack frame. Windows x86 has a separate
// window below. If a future toolchain emits longer bodies, the relevant
// platform contract and readable-range guarantee must grow together, and the
// decoder must still terminate on `ret` within that window.
constexpr size_t kGetterCodeWindowBytes = 16;

// Electron 43 win32-ia32 emits an 18-byte framed __thiscall sret getter. Keep
// that larger contract isolated to the 32-bit Windows parser and vtable scan;
// other Windows architectures retain the existing 16-byte arbitrary-code read.
constexpr size_t kX86GetterCodeWindowBytes = 24;

// Second, larger window for hardened builds, used only when a matcher has
// already failed at kGetterCodeWindowBytes and the caller has confirmed this
// many bytes are readable. Linux distributions compile Node with hardening flags
// that wrap the same one-instruction field load in a longer prologue and
// epilogue, pushing `ret` past the window above:
//
//   aarch64, `-mbranch-protection=standard` (Fedora) — 28 bytes:
//     paciasp; stp x29,x30,[sp,#-16]!; mov x29,sp;
//     ldr x0,[x0,#imm]; ldp x29,x30,[sp],#16; autiasp; ret
//
//   x86-64, `-fcf-protection` plus a frame pointer — 17 bytes:
//     endbr64; push rbp; mov rbp,rsp;
//     mov rax,[rdi+disp32]; pop rbp; ret
//
// Both architectures therefore get the same retry-in-a-wider-window treatment
// (see the linux_glibc_* parsers). Growing kGetterCodeWindowBytes itself would
// lengthen the memcpy every matcher performs on every platform, including the
// Windows vtable scanner that walks arbitrary code pointers, so the longer
// bodies get their own opt-in window instead.
constexpr size_t kHardenedGetterCodeWindowBytes = 32;

// The builtin_module_require getter is a tiny accessor that loads a pointer
// field from `this` (the Realm) at a fixed offset and returns it. Rather than
// comparing a fixed run of raw opcode bytes, each architecture decodes the
// getter body into a short instruction list and then matches the sequence
// semantically. This tolerates benign prologue/epilogue variation (frame
// pointer, CET `endbr64`, BTI landing pads, MSVC struct-return thunks) while
// still failing closed on anything that is not a recognized field getter.
//
// `platform_tag` is a short label (e.g. "darwin-x64") woven into the returned
// GetterPattern::pattern for diagnostics; the matching logic itself depends
// only on the calling convention, not the operating system.

// x86-64 System V (Linux, macOS): `this` in RDI, result returned in RAX.
//
// `window_bytes` is how many bytes the caller has confirmed are readable at
// `getter`, clamped to kHardenedGetterCodeWindowBytes. It defaults to the
// standard window, so callers that do not care about hardened builds are
// unaffected; pass the larger window to retry a body whose `ret` fell outside
// the standard one.
Result<GetterPattern> MatchX64SysVFieldGetter(
    void* getter,
    std::string_view platform_tag,
    size_t window_bytes = kGetterCodeWindowBytes);

// x86-64 Windows: `this` in RCX; large return types use a hidden struct-return
// pointer, shifting `this` and adding a store-through-sret epilogue.
Result<GetterPattern> MatchX64Win64FieldGetter(void* getter,
                                               std::string_view platform_tag);

// AArch64: `this` in X0, result in X0, with an optional leading `bti c` landing
// pad on BTI-enabled builds. Matches at fixed positions, so it accepts only that
// two-or-three instruction shape.
//
// Used by darwin-arm64, where every Node in practice is an official nodejs.org
// build or an Apple-clang build with default flags, and both emit exactly this.
// Apple platforms cannot reach the hardened shapes anyway: pointer
// authentication there requires the arm64e ABI, which third-party code does not
// target, and BTI is a Linux/Android mechanism. If an official macOS build ever
// changes compiler flags, switch that parser to MatchArm64AapcsFieldGetter — the
// archived apple-clang fixtures in the decoder self-test already cover the
// shapes that change would produce, including `retaa`.
Result<GetterPattern> MatchArm64FieldGetter(void* getter,
                                            std::string_view platform_tag);

// AArch64 AAPCS: `this` in X0, result in X0 — the same getter and ABI as
// MatchArm64FieldGetter, but walked one instruction at a time in the style of
// MatchX64SysVFieldGetter/WalkX64Getter instead of matched at fixed positions.
// The walking is the point: it tolerates benign prologue and epilogue variation
// the way the x86-64 matcher always has, which is why hardened x86-64 builds
// were never broken by distribution hardening while AArch64 was.
//
// Used by linux-arm64 for both stages of its two-stage decode, mirroring
// linux_glibc_x64.cc. Because it walks, the hardened shapes that fit inside
// kGetterCodeWindowBytes are resolved in the first stage and never need the wider
// window: `paciasp; ldr; retaa` (clang) is 12 bytes and
// `paciasp; ldr; autiasp; ret` (GCC) is exactly 16, in either instruction order.
//
// `window_bytes` is how many bytes the caller has confirmed are readable at
// `getter`; it is clamped to kHardenedGetterCodeWindowBytes.
//
// Prologue and epilogue instructions are matched by their exact 32-bit encodings
// where they are operand-free, and by a pinned-register decode where they are not
// (the frame record fixes Rt=x29/Rt2=x30/Rn=sp but leaves the frame size free —
// see IsArm64FrameRecordInsn). Exactly one `ldr x0,[x0,#imm]` is permitted and the
// body must terminate with `ret`, `retaa`, or `retab`. Anything else fails closed.
//
// The returned pattern text is byte-identical to MatchArm64FieldGetter's for
// every shape that function accepts, so routing a platform through this walker
// cannot change the diagnostics of a getter that already matched.
Result<GetterPattern> MatchArm64AapcsFieldGetter(void* getter,
                                                 std::string_view platform_tag,
                                                 size_t window_bytes);

// AArch64 Windows (MSVC): `this` in X0; a non-trivial return type uses a hidden
// struct-return pointer in X1, so the field is loaded from X0 and stored through
// X1 rather than returned directly. Accepts both that sret form and the direct
// X0-return form, selecting the call mode accordingly.
Result<GetterPattern> MatchArm64Win64FieldGetter(void* getter,
                                                 std::string_view platform_tag);

// x86 (32-bit) Windows (MSVC __thiscall): `this` in ECX. A pointer/scalar
// return comes back in EAX from a bare `ret`; a non-trivial return uses a hidden
// struct-return pointer passed on the stack, so the callee cleans it up with
// `ret 4`. Selects the call mode from that terminating ret form. Reads the
// dedicated kX86GetterCodeWindowBytes window.
Result<GetterPattern> MatchX86ThiscallFieldGetter(void* getter,
                                                  std::string_view platform_tag);

}  // namespace esplus::node::require_builtin
