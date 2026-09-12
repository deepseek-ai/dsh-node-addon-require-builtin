#include "runtime_profile.h"

#include <array>
#include <string>

namespace esplus::node::require_builtin {
namespace {

constexpr uint16_t kElectronEmbedderDataTag = 0;

struct ElectronProfileSpec {
  std::string_view electron_version;
  uint32_t node_major;
  uint32_t node_minor;
  uint32_t node_patch;
  std::string_view v8_version;
  const char* diagnostic_name;
};

constexpr std::array<ElectronProfileSpec, 3> kElectronProfiles = {{
    {"43.0.0", 24, 17, 0, "15.0.245.13-electron.0",
     "electron-43 tagged default=0"},
    {"44.0.0", 24, 18, 1, "15.2.124.13-electron.0",
     "electron-44 tagged default=0"},
    {"45.0.0-alpha.6", 24, 21, 0, "15.4.80-electron.0",
     "electron-45-alpha tagged default=0"},
}};

bool IsElectronV8Version(std::string_view version) {
  return version.find("-electron.") != std::string_view::npos;
}

bool Matches(const ElectronProfileSpec& profile,
             const NativeRuntimeFingerprint& fingerprint) {
  return profile.node_major == fingerprint.node_major &&
      profile.node_minor == fingerprint.node_minor &&
      profile.node_patch == fingerprint.node_patch &&
      profile.v8_version == fingerprint.v8_version;
}

void AppendNodeVersion(std::string* output,
                       const NativeRuntimeFingerprint& fingerprint) {
  output->append(std::to_string(fingerprint.node_major));
  output->push_back('.');
  output->append(std::to_string(fingerprint.node_minor));
  output->push_back('.');
  output->append(std::to_string(fingerprint.node_patch));
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
    const NativeRuntimeFingerprint& fingerprint) {
  for (const auto& profile : kElectronProfiles) {
    if (Matches(profile, fingerprint)) {
      // Electron 43-45 embed Node 24 with a newer tagged V8 API, while their
      // Node-side context setup still stores Realm with the default tag 0.
      return Result<NapiRuntimeProfile>::Ok(NapiRuntimeProfile(
          EmbedderDataAbi::kTagged,
          kElectronEmbedderDataTag,
          profile.diagnostic_name));
    }
  }

  std::string message = "unsupported Electron runtime fingerprint: Node ";
  AppendNodeVersion(&message, fingerprint);
  message.append(", V8 ");
  message.append(fingerprint.v8_version);
  message.append(" (supported Electron versions: ");
  for (size_t index = 0; index < kElectronProfiles.size(); ++index) {
    if (index != 0) message.append(", ");
    message.append(kElectronProfiles[index].electron_version);
  }
  message.push_back(')');
  return Result<NapiRuntimeProfile>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoContext, message));
}

Result<NapiRuntimeProfile> NapiRuntimeProfile::FromFingerprint(
    const NativeRuntimeFingerprint& fingerprint) {
  if (IsElectronV8Version(fingerprint.v8_version)) {
    return Electron(fingerprint);
  }
  return Result<NapiRuntimeProfile>::Ok(Node(fingerprint.node_major));
}

}  // namespace esplus::node::require_builtin
