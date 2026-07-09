#pragma once

#include "native_types.h"

#include <napi.h>

namespace esplus::node::require_builtin {

enum class ProbeOutcome {
  kContinue,
  kPartial,
};

class RequireBuiltinProbe {
 public:
  RequireBuiltinProbe(Napi::Env env, Napi::Value target, bool load_target);

  bool Run();
  const ProbeState& state() const { return state_; }
  const Diagnostics& diagnostics() const { return diagnostics_; }

 private:
  Status RunStatus();
  Status ResolveRequireBuiltin();
  Status ValidateRequireBuiltinName();
  Status SmokeTest();
  bool HasSelfReference(Napi::Object object,
                        const char* property,
                        SmokePropertyKind property_kind);
  Result<ProbeOutcome> LoadTargetModule();
  Napi::Value GetNamedProperty(Napi::Value object, const char* key);
  std::string NapiStringToStdString(Napi::Value value);

  Napi::Env env_;
  Napi::String target_id_;
  bool load_target_;
  ProbeState state_;
  Diagnostics diagnostics_;
};

Napi::Value CallRequireBuiltin(Napi::Function require_builtin, Napi::String id);

}  // namespace esplus::node::require_builtin
