#pragma once

#include "../native_types.h"

namespace esplus::node::require_builtin {

enum class EmbedderDataAbi {
  kUntagged,
  kTagged,
};

class NapiRuntimeProfile final {
 public:
  static Result<NapiRuntimeProfile> Detect(napi_env env);

  EmbedderDataAbi embedder_data_abi() const { return embedder_data_abi_; }
  uint16_t embedder_data_tag() const { return embedder_data_tag_; }
  const char* diagnostic_name() const { return diagnostic_name_; }

 private:
  static NapiRuntimeProfile Node(uint32_t major);
  static Result<NapiRuntimeProfile> Electron(std::string_view version);

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
