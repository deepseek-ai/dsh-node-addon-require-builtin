#pragma once

#include "backend_config.h"
#include "product_config.h"
#include "debug_trace.h"

#include <node_api.h>

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace esplus::node::require_builtin {

struct RuntimeRequireBuiltin;

// Calling convention of a non-static C++ member function. On 32-bit x86 (MSVC)
// that is __thiscall: `this` is passed in ECX and the callee cleans up its
// stack arguments (`ret imm16`). On x86-64 and AArch64 the member ABI is folded
// into the single platform convention, so this expands to nothing there. The
// private V8 member functions the probe resolves at runtime must be called
// through pointers carrying this convention, or the x86 call would push `this`
// on the stack and corrupt the frame.
#if defined(_WIN32) && defined(_M_IX86)
#define NARB_MEMBER_ABI __thiscall
#else
#define NARB_MEMBER_ABI
#endif

constexpr int kRealmSlot = 38;
constexpr uint16_t kPerContextDataTag = 2;
constexpr std::string_view kCjsLoaderTarget = "internal/modules/cjs/loader";
constexpr std::string_view kEsmLoaderTarget = "internal/modules/esm/loader";
constexpr std::string_view kDefaultTarget = kEsmLoaderTarget;

enum class ProbeStatus {
  kUnsupportedNotStarted,
  kSupported,
  kPartialRequireBuiltinOnly,
  kUnsupportedNoContext,
  kUnsupportedNoRealm,
  kUnsupportedNoGetter,
  kUnsupportedHandleMode,
  kUnsupportedDisallowedTarget,
};

enum class ProbeResultKind {
  kUnsupported,
  kSupported,
  kPartial,
};

enum class SmokePropertyKind {
  kNone,
  kRequire,
  kRequireBuiltin,
};

struct Diagnostics {
  std::string status = "Unsupported/not-started";
  std::string result = "unsupported";
  std::string error;
  std::string node;
  uint32_t napi_version = 0;
  std::string backend = std::string{kBackend};
  std::string binary_abi;
  std::string mode = std::string{kMode};
  std::string product = std::string{kProduct};
  std::string platform;
  std::string arch;
  std::string embedder_data;
  std::string getter_symbol = "not-found";
  std::string getter_symbol_name;
  std::string getter_pattern = "none";
  std::string getter_image;
  std::string vptr_image;
  std::string require_builtin_name;
  std::string smoke_property;
  std::string target = std::string{kDefaultTarget};
  uintptr_t isolate = 0;
  uintptr_t context = 0;
  uintptr_t realm = 0;
  uintptr_t vptr = 0;
  uintptr_t getter = 0;
  size_t offset = 0;
  uint32_t embedder_fields = 0;
  bool uses_node_addon_api = true;
  bool has_v8_context = false;
  bool target_exports_ok = false;
};

struct ProbeState {
  void ApplyRuntimeRequireBuiltin(const RuntimeRequireBuiltin& runtime);
  Diagnostics ToDiagnostics(napi_env env) const;

  ProbeStatus status = ProbeStatus::kUnsupportedNotStarted;
  ProbeResultKind result = ProbeResultKind::kUnsupported;
  SmokePropertyKind smoke_property = SmokePropertyKind::kNone;
  std::string error;
  std::string target = std::string{kDefaultTarget};
  std::string require_builtin_name;
  std::string embedder_data;
  std::string getter_symbol = "not-found";
  std::string getter_symbol_name;
  std::string getter_pattern = "none";
  std::string getter_image;
  std::string vptr_image;
  uintptr_t isolate = 0;
  uintptr_t context = 0;
  uintptr_t realm = 0;
  uintptr_t vptr = 0;
  uintptr_t getter = 0;
  size_t offset = 0;
  uint32_t embedder_fields = 0;
  napi_value require_builtin = nullptr;
  napi_value target_exports = nullptr;
  bool target_exports_ok = false;
  bool has_v8_context = false;
};

class Status {
 public:
  static Status Ok() { return Status(true, ProbeStatus::kSupported, ""); }
  static Status Failure(ProbeStatus code, std::string_view message) {
    return Status(false, code, message);
  }

  bool ok() const { return ok_; }
  ProbeStatus code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  Status(bool ok, ProbeStatus code, std::string_view message)
      : ok_(ok), code_(code), message_(message) {}

  bool ok_;
  ProbeStatus code_;
  std::string message_;
};

template <typename T>
class Result {
 public:
  static Result Ok(T value) {
    return Result(std::move(value));
  }

  static Result Failure(Status status) {
    return Result(std::move(status));
  }

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  const T& value() const {
    if (!value_.has_value()) std::terminate();
    return *value_;
  }
  T& value() {
    if (!value_.has_value()) std::terminate();
    return *value_;
  }

 private:
  explicit Result(T value)
      : value_(std::move(value)), status_(Status::Ok()) {}
  explicit Result(Status status)
      : value_(std::nullopt), status_(std::move(status)) {
    assert(!status_.ok());
  }

  std::optional<T> value_;
  Status status_;
};

std::string Hex(uintptr_t value);
std::string PlatformName();
std::string ArchName();
std::string NodeVersion(napi_env env);
uint32_t NapiVersion(napi_env env);
std::string BinaryAbi();
bool IsPointerAligned(uintptr_t value);
bool IsAllowedInternalId(std::string_view module_id);
std::string_view AllowedInternalIdList();
std::string_view ProbeStatusString(ProbeStatus status);
std::string_view ProbeResultString(ProbeResultKind result);
std::string_view SmokePropertyString(SmokePropertyKind property);

}  // namespace esplus::node::require_builtin
