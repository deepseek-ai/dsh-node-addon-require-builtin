#pragma once

#include "../native_types.h"

namespace esplus::node::require_builtin {

enum class EmbedderDataAbi {
  kUntagged,
  kTagged,
};

struct NativeRuntimeFingerprint {
  uint32_t node_major = 0;
  uint32_t node_minor = 0;
  uint32_t node_patch = 0;
  std::string_view v8_version;
};

class NapiRuntimeProfile final {
 public:
  static Result<NapiRuntimeProfile> Detect(napi_env env);
  static Result<NapiRuntimeProfile> FromFingerprint(
      const NativeRuntimeFingerprint& fingerprint);

  EmbedderDataAbi embedder_data_abi() const { return embedder_data_abi_; }
  uint16_t embedder_data_tag() const { return embedder_data_tag_; }
  const char* diagnostic_name() const { return diagnostic_name_; }

 private:
  static NapiRuntimeProfile Node(uint32_t major);
  static Result<NapiRuntimeProfile> Electron(
      const NativeRuntimeFingerprint& fingerprint);

  NapiRuntimeProfile(EmbedderDataAbi embedder_data_abi,
                     uint16_t embedder_data_tag,
                     const char* diagnostic_name)
      : embedder_data_abi_(embedder_data_abi),
        embedder_data_tag_(embedder_data_tag),
        diagnostic_name_(diagnostic_name) {}

  EmbedderDataAbi embedder_data_abi_;
  uint16_t embedder_data_tag_;
  const char* diagnostic_name_;
};

}  // namespace esplus::node::require_builtin
