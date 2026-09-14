#include "runtime_context/runtime_profile.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace narb = esplus::node::require_builtin;

namespace {

[[noreturn]] void Fail(std::string_view label, std::string_view message) {
  std::cerr << "not ok " << label << ": " << message << '\n';
  std::exit(1);
}

void ExpectProfile(std::string_view label,
                   const narb::NativeRuntimeFingerprint& fingerprint,
                   narb::EmbedderDataAbi expected_abi,
                   uint16_t expected_tag,
                   std::string_view expected_diagnostic) {
  auto result = narb::NapiRuntimeProfile::FromFingerprint(fingerprint);
  if (!result.ok()) Fail(label, result.status().message());
  const auto& profile = result.value();
  if (profile.embedder_data_abi() != expected_abi) {
    Fail(label, "unexpected embedder-data ABI");
  }
  if (profile.embedder_data_tag() != expected_tag) {
    Fail(label, "unexpected embedder-data tag");
  }
  if (std::string_view(profile.diagnostic_name()) != expected_diagnostic) {
    Fail(label, "unexpected diagnostic name");
  }
  std::cout << "ok   " << label << '\n';
}

void ExpectUnsupported(std::string_view label,
                       const narb::NativeRuntimeFingerprint& fingerprint) {
  auto result = narb::NapiRuntimeProfile::FromFingerprint(fingerprint);
  if (result.ok()) Fail(label, "fingerprint was accepted");
  if (result.status().code() != narb::ProbeStatus::kUnsupportedNoContext) {
    Fail(label, "unexpected failure status");
  }
  if (result.status().message().find("unsupported Electron runtime fingerprint") ==
      std::string::npos) {
    Fail(label, "unexpected failure diagnostic");
  }
  std::cout << "ok   " << label << " rejected as expected\n";
}

}  // namespace

int main() {
  ExpectProfile(
      "Node 20 ignores ordinary V8 build metadata",
      {20, 20, 2, "11.3.244.8-node.33"},
      narb::EmbedderDataAbi::kUntagged,
      0,
      "untagged Node 20/22/24 family");
  ExpectProfile(
      "Node 24 remains on the untagged path",
      {24, 18, 0, "13.6.233.17-node.37"},
      narb::EmbedderDataAbi::kUntagged,
      0,
      "untagged Node 20/22/24 family");
  ExpectProfile(
      "Node 26 remains on tag 2",
      {26, 4, 0, "14.3.127.17-node.10"},
      narb::EmbedderDataAbi::kTagged,
      narb::kPerContextDataTag,
      "tagged kPerContextData=2");

  ExpectProfile(
      "Electron 43 native fingerprint",
      {24, 17, 0, "15.0.245.13-electron.0"},
      narb::EmbedderDataAbi::kTagged,
      0,
      "electron-43 tagged default=0");
  ExpectProfile(
      "Electron 44 native fingerprint",
      {24, 18, 1, "15.2.124.13-electron.0"},
      narb::EmbedderDataAbi::kTagged,
      0,
      "electron-44 tagged default=0");
  ExpectProfile(
      "Electron 45 native fingerprint",
      {24, 21, 0, "15.4.80-electron.0"},
      narb::EmbedderDataAbi::kTagged,
      0,
      "electron-45-alpha tagged default=0");

  ExpectUnsupported(
      "unknown Electron V8 version",
      {24, 21, 0, "15.4.81-electron.0"});
  ExpectUnsupported(
      "known Electron V8 with wrong native Node version",
      {24, 21, 1, "15.4.80-electron.0"});

  std::cout << "\nAll runtime-profile cases passed.\n";
  return 0;
}
