#ifndef INTERNAL_REQUIRE_RUNTIME_PROBE_GETTER_DECODER_H_
#define INTERNAL_REQUIRE_RUNTIME_PROBE_GETTER_DECODER_H_

#include "../runtime_compat.h"

#include <string_view>

namespace internal_require {

// Number of bytes each matcher copies out of a candidate getter before
// decoding. Callers that hand the matchers an arbitrary code pointer (e.g. the
// Windows vtable scanner) must ensure at least this many bytes are readable.
constexpr size_t kGetterCodeWindowBytes = 16;

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
Result<GetterPattern> MatchX64SysVFieldGetter(void* getter,
                                              std::string_view platform_tag);

// x86-64 Windows: `this` in RCX; large return types use a hidden struct-return
// pointer, shifting `this` and adding a store-through-sret epilogue.
Result<GetterPattern> MatchX64Win64FieldGetter(void* getter,
                                               std::string_view platform_tag);

// AArch64 (all operating systems): `this` in X0, result in X0, with an optional
// leading `bti c` landing pad on BTI-enabled builds.
Result<GetterPattern> MatchArm64FieldGetter(void* getter,
                                            std::string_view platform_tag);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_RUNTIME_PROBE_GETTER_DECODER_H_
