#ifndef INTERNAL_REQUIRE_DEBUG_TRACE_H_
#define INTERNAL_REQUIRE_DEBUG_TRACE_H_

#include <cstddef>

namespace internal_require {

#if defined(__GNUC__) || defined(__clang__)
#define INTERNAL_REQUIRE_PRINTF_FORMAT(format_index, first_arg_index) \
  __attribute__((format(printf, format_index, first_arg_index)))
#else
#define INTERNAL_REQUIRE_PRINTF_FORMAT(format_index, first_arg_index)
#endif

// Env-gated native tracing. The probe crosses private Node/V8 ABI boundaries
// where a mismatch faults before any JS error can be thrown, so stderr tracing
// is the only way to localize a hard crash on CI.
//
// Enabled when DSH_NODE_ADDON_INTERNAL_TRACE is set to a value whose first
// character is neither '\0' nor '0' (e.g. "1"). The result is cached on first
// use, so the environment is read exactly once per process.
bool DebugTraceEnabled();

// Formats one trace line to stderr (prefixed and newline-terminated) when
// tracing is enabled. No-op otherwise. The format string is checked against its
// arguments by the compiler.
void DebugTrace(const char* format, ...) INTERNAL_REQUIRE_PRINTF_FORMAT(1, 2);

// Traces up to `length` bytes at `data` as space-separated hex. `data` must
// point at an already-copied, readable local buffer; this helper never
// dereferences arbitrary runtime addresses itself.
void DebugTraceBytes(const char* label, const void* data, size_t length);

}  // namespace internal_require

#endif  // INTERNAL_REQUIRE_DEBUG_TRACE_H_
