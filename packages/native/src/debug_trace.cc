#include "debug_trace.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esplus::node::require_builtin {
namespace {

constexpr const char* kTraceEnvVar = "NARB_TRACE";
constexpr const char* kTracePrefix = "[NARB::TRACE] ";

}  // namespace

bool DebugTraceEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv(kTraceEnvVar);
    return value != nullptr && value[0] != '\0' && value[0] != '0';
  }();
  return enabled;
}

void DebugTrace(const char* format, ...) {
  if (!DebugTraceEnabled()) return;
  std::fputs(kTracePrefix, stderr);
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
  // Flush eagerly: a following ABI call may crash before normal teardown.
  std::fflush(stderr);
}

void DebugTraceBytes(const char* label, const void* data, size_t length) {
  if (!DebugTraceEnabled() || data == nullptr || length == 0) return;

  // Render into a single stack buffer so the whole line is emitted by one
  // DebugTrace call. Three chars per byte ("ff ") plus a terminator.
  constexpr size_t kMaxBytes = 32;
  const size_t count = length < kMaxBytes ? length : kMaxBytes;
  const auto* bytes = static_cast<const unsigned char*>(data);

  char hex[kMaxBytes * 3 + 1];
  size_t pos = 0;
  for (size_t i = 0; i < count; ++i) {
    std::snprintf(hex + pos, sizeof(hex) - pos, "%02x ", bytes[i]);
    pos += 3;
  }
  if (pos > 0) hex[pos - 1] = '\0';  // drop trailing space

  DebugTrace("%s bytes: %s", label, hex);
}

}  // namespace esplus::node::require_builtin
