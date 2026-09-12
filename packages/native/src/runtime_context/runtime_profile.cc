#include "runtime_profile.h"

#include <array>
#include <string>

namespace esplus::node::require_builtin {
namespace {

constexpr uint16_t kElectronEmbedderDataTag = 0;

struct ElectronProfileSpec {
  std::string_view version;
  const char* diagnostic_name;
};

constexpr std::array<ElectronProfileSpec, 3> kElectronProfiles = {{
    {"43.0.0", "electron-43 tagged default=0"},
    {"44.0.0", "electron-44 tagged default=0"},
    {"45.0.0-alpha.6", "electron-45-alpha tagged default=0"},
}};

struct ElectronVersion {
  bool present = false;
  std::string value;
};

Result<ElectronVersion> ReadElectronVersion(napi_env env) {
  napi_value global = nullptr;
  napi_value process = nullptr;
  napi_value versions = nullptr;
  bool has_electron = false;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr ||
      napi_get_named_property(env, global, "process", &process) != napi_ok ||
      process == nullptr ||
      napi_get_named_property(env, process, "versions", &versions) != napi_ok ||
      versions == nullptr ||
      napi_has_named_property(env, versions, "electron", &has_electron) != napi_ok) {
    return Result<ElectronVersion>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "could not inspect process.versions for runtime host"));
  }
  if (!has_electron) return Result<ElectronVersion>::Ok({});

  napi_value electron = nullptr;
  napi_valuetype type = napi_undefined;
  if (napi_get_named_property(env, versions, "electron", &electron) != napi_ok ||
      electron == nullptr || napi_typeof(env, electron, &type) != napi_ok ||
      type != napi_string) {
    return Result<ElectronVersion>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "process.versions.electron is not a string"));
  }

  size_t size = 0;
  if (napi_get_value_string_utf8(env, electron, nullptr, 0, &size) != napi_ok) {
    return Result<ElectronVersion>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "could not read process.versions.electron"));
  }
  std::string value(size + 1, '\0');
  if (napi_get_value_string_utf8(
          env, electron, value.data(), value.size(), &size) != napi_ok) {
    return Result<ElectronVersion>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "could not read process.versions.electron"));
  }
  value.resize(size);
  return Result<ElectronVersion>::Ok({true, std::move(value)});
}

}  // namespace

NapiRuntimeProfile NapiRuntimeProfile::Node(uint32_t major) {
  // Preserve the pre-Electron Node behavior exactly: Node 26+ uses the tagged
  // V8 overload and kPerContextData=2; older supported Node versions use the
  // untagged overload.
  if (major >= 26) {
    return NapiRuntimeProfile(
        EmbedderDataAbi::kTagged,
        kPerContextDataTag,
        "tagged kPerContextData=2");
  }
  return NapiRuntimeProfile(
      EmbedderDataAbi::kUntagged, 0, "untagged Node 20/22/24 family");
}

Result<NapiRuntimeProfile> NapiRuntimeProfile::Electron(
    std::string_view version) {
  for (const auto& profile : kElectronProfiles) {
    if (version == profile.version) {
      // Electron 43-45 embed Node 24 with a newer tagged V8 API, while their
      // Node-side context setup still stores Realm with the default tag 0.
      return Result<NapiRuntimeProfile>::Ok(NapiRuntimeProfile(
          EmbedderDataAbi::kTagged,
          kElectronEmbedderDataTag,
          profile.diagnostic_name));
    }
  }

  std::string message = "unsupported Electron version: ";
  message.append(version);
  message.append(" (supported: ");
  for (size_t index = 0; index < kElectronProfiles.size(); ++index) {
    if (index != 0) message.append(", ");
    message.append(kElectronProfiles[index].version);
  }
  message.push_back(')');
  return Result<NapiRuntimeProfile>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoContext, message));
}

Result<NapiRuntimeProfile> NapiRuntimeProfile::Detect(napi_env env) {
  auto electron_version = ReadElectronVersion(env);
  if (!electron_version.ok()) {
    return Result<NapiRuntimeProfile>::Failure(electron_version.status());
  }
  if (electron_version.value().present) {
    return Electron(electron_version.value().value);
  }

  const napi_node_version* version = nullptr;
  if (napi_get_node_version(env, &version) != napi_ok || version == nullptr) {
    return Result<NapiRuntimeProfile>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext, "could not read Node.js version"));
  }
  return Result<NapiRuntimeProfile>::Ok(Node(version->major));
}

}  // namespace esplus::node::require_builtin
