#include "runtime_profile.h"

#include "../runtime_symbol.h"

namespace esplus::node::require_builtin {
namespace {

constexpr std::string_view kSymV8GetVersion = "_ZN2v82V810GetVersionEv";

using GetV8VersionFn = const char* (*)();

}  // namespace

Result<NapiRuntimeProfile> NapiRuntimeProfile::Detect(napi_env env) {
  const napi_node_version* node_version = nullptr;
  if (napi_get_node_version(env, &node_version) != napi_ok ||
      node_version == nullptr) {
    return Result<NapiRuntimeProfile>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext, "could not read Node.js version"));
  }

  auto get_v8_version =
      LookupProcessFunction<GetV8VersionFn>(kSymV8GetVersion);
  if (get_v8_version == nullptr) {
    return Result<NapiRuntimeProfile>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "V8::GetVersion symbol not found"));
  }

  const char* v8_version = get_v8_version();
  if (v8_version == nullptr || v8_version[0] == '\0') {
    return Result<NapiRuntimeProfile>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "V8::GetVersion returned an empty version"));
  }

  DebugTrace("native runtime fingerprint: Node %u.%u.%u, V8 %s",
             node_version->major,
             node_version->minor,
             node_version->patch,
             v8_version);
  return FromFingerprint({
      node_version->major,
      node_version->minor,
      node_version->patch,
      v8_version,
  });
}

}  // namespace esplus::node::require_builtin
