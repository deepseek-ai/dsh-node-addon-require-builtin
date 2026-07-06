#include "native_types.h"

#include "runtime_compat.h"

#include <node_version.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace internal_require {

bool TraceEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("DSH_NODE_ADDON_INTERNAL_TRACE");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
  }();
  return enabled;
}

void TracePrintf(const char* format, ...) {
  if (!TraceEnabled()) return;
  std::fputs("[dsh-probe] ", stderr);
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
  // Flush eagerly: a following ABI call may crash before normal teardown.
  std::fflush(stderr);
}

void ProbeState::ApplyRuntimeRequireBuiltin(
    const RuntimeRequireBuiltin& runtime) {
  isolate = runtime.context.isolate;
  context = runtime.context.context;
  realm = runtime.context.realm;
  vptr = runtime.image.vptr;
  getter = runtime.getter.address;
  offset = runtime.pattern.offset;
  embedder_fields = runtime.context.embedder_fields;
  embedder_data = runtime.context.embedder_data;
  getter_symbol = runtime.getter.symbol;
  getter_symbol_name = runtime.getter.symbol_name;
  getter_pattern = runtime.pattern.pattern;
  getter_image = runtime.image.getter_image;
  vptr_image = runtime.image.vptr_image;
  has_v8_context = runtime.context.has_v8_context;
}

Diagnostics ProbeState::ToDiagnostics(napi_env env) const {
  Diagnostics diagnostics;
  diagnostics.node = NodeVersion(env);
  diagnostics.napi_version = NapiVersion(env);
  diagnostics.backend = kBackend;
  diagnostics.binary_abi = BinaryAbi();
  diagnostics.platform = PlatformName();
  diagnostics.arch = ArchName();
  diagnostics.embedder_data = embedder_data;
  diagnostics.getter_symbol = getter_symbol;
  diagnostics.getter_symbol_name = getter_symbol_name;
  diagnostics.getter_pattern = getter_pattern;
  diagnostics.getter_image = getter_image;
  diagnostics.vptr_image = vptr_image;
  diagnostics.target = target;
  diagnostics.status = ProbeStatusString(status);
  diagnostics.result = ProbeResultString(result);
  diagnostics.error = error;
  diagnostics.require_builtin_name = require_builtin_name;
  diagnostics.smoke_property = SmokePropertyString(smoke_property);
  diagnostics.target_exports_ok = target_exports_ok;
  diagnostics.isolate = isolate;
  diagnostics.context = context;
  diagnostics.realm = realm;
  diagnostics.vptr = vptr;
  diagnostics.getter = getter;
  diagnostics.offset = offset;
  diagnostics.embedder_fields = embedder_fields;
  diagnostics.has_v8_context = has_v8_context;
  return diagnostics;
}

std::string Hex(uintptr_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << value;
  return out.str();
}

std::string PlatformName() {
#if defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "win32";
#else
  return "unknown";
#endif
}

std::string ArchName() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x64";
#elif defined(__arm__)
  return "arm";
#elif defined(__i386__) || defined(_M_IX86)
  return "ia32";
#else
  return "unknown";
#endif
}

std::string NodeVersion(napi_env env) {
  const napi_node_version* version = nullptr;
  if (napi_get_node_version(env, &version) != napi_ok || version == nullptr) {
    return "";
  }

  std::ostringstream out;
  out << version->major << "." << version->minor << "." << version->patch;
  return out.str();
}

uint32_t NapiVersion(napi_env env) {
  uint32_t version = 0;
  napi_get_version(env, &version);
  return version;
}

std::string BinaryAbi() {
  std::ostringstream out;
#if INTERNAL_REQUIRE_BACKEND == INTERNAL_REQUIRE_BACKEND_NAPI
  out << "napi-v" << NAPI_VERSION;
#else
  out << "node-v" << NODE_MODULE_VERSION;
#endif
  return out.str();
}

bool IsPointerAligned(uintptr_t value) {
  return value != 0 && (value % alignof(void*)) == 0;
}

bool IsAllowedTarget(std::string_view target) {
  return target == kCjsLoaderTarget || target == kEsmLoaderTarget;
}

std::string_view AllowedTargetList() {
  return "internal/modules/cjs/loader, internal/modules/esm/loader";
}

std::string_view ProbeStatusString(ProbeStatus status) {
  switch (status) {
    case ProbeStatus::kUnsupportedNotStarted:
      return "Unsupported/not-started";
    case ProbeStatus::kSupported:
      return "Supported";
    case ProbeStatus::kPartialInternalRequireOnly:
      return "Partial/internal-require-only";
    case ProbeStatus::kUnsupportedNoContext:
      return "Unsupported/no-context";
    case ProbeStatus::kUnsupportedNoRealm:
      return "Unsupported/no-realm";
    case ProbeStatus::kUnsupportedNoGetter:
      return "Unsupported/no-getter";
    case ProbeStatus::kUnsupportedHandleMode:
      return "Unsupported/handle-mode";
    case ProbeStatus::kUnsupportedDisallowedTarget:
      return "Unsupported/disallowed-target";
  }
  return "Unsupported/unknown";
}

std::string_view ProbeResultString(ProbeResultKind result) {
  switch (result) {
    case ProbeResultKind::kUnsupported:
      return "unsupported";
    case ProbeResultKind::kSupported:
      return "supported";
    case ProbeResultKind::kPartial:
      return "partial";
  }
  return "unsupported";
}

std::string_view SmokePropertyString(SmokePropertyKind property) {
  switch (property) {
    case SmokePropertyKind::kNone:
      return "";
    case SmokePropertyKind::kRequire:
      return "require";
    case SmokePropertyKind::kRequireBuiltin:
      return "requireBuiltin";
  }
  return "";
}

}  // namespace internal_require
