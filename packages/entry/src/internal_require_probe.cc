#include "internal_require_probe.h"

#include "runtime_compat.h"

#include <utility>

namespace internal_require {

InternalRequireProbe::InternalRequireProbe(Napi::Env env,
                                           Napi::Value target,
                                           bool load_target)
    : env_(env), load_target_(load_target) {
  if (target.IsString()) {
    Napi::String candidate = target.As<Napi::String>();
    const std::string candidate_text = candidate.Utf8Value();
    if (!candidate_text.empty()) {
      target_id_ = candidate;
      state_.target = candidate_text;
      return;
    }
  }

  if (load_target_) target_id_ = Napi::String::New(env_, kDefaultTarget);
  state_.target.assign(kDefaultTarget.data(), kDefaultTarget.size());
}

Napi::Value InternalRequireProbe::GetNamedProperty(Napi::Value object,
                                                   const char* key) {
  if (object.IsEmpty() || !object.IsObject()) return {};
  return object.As<Napi::Object>().Get(key);
}

std::string InternalRequireProbe::NapiStringToStdString(Napi::Value value) {
  if (value.IsEmpty() || !value.IsString()) return "";
  return value.As<Napi::String>().Utf8Value();
}

Status InternalRequireProbe::ResolveRequireBuiltin() {
  // Version-specific private probing is isolated in the selected runtime_compat
  // backend implementation. From this point on the flow uses node-addon-api
  // wrappers for public JS value operations.
  auto runtime = ProbeRuntimeRequireBuiltin(env_);
  if (!runtime.ok()) return runtime.status();
  state_.ApplyRuntimeRequireBuiltin(runtime.value());
  state_.require_builtin = runtime.value().value;
  return Status::Ok();
}

Status InternalRequireProbe::ValidateRequireBuiltinName() {
  Napi::Value name_value = GetNamedProperty(
      Napi::Function(env_, state_.require_builtin), "name");
  state_.require_builtin_name = NapiStringToStdString(name_value);
  if (state_.require_builtin_name != "requireBuiltin") {
    return Status::Failure(ProbeStatus::kUnsupportedNoGetter,
                           "Realm field function name is not requireBuiltin");
  }
  return Status::Ok();
}

bool InternalRequireProbe::HasSelfReference(Napi::Object object,
                                            const char* property,
                                            SmokePropertyKind property_kind) {
  Napi::Value value = GetNamedProperty(object, property);
  if (value.IsEmpty()) return false;

  if (value.StrictEquals(Napi::Function(env_, state_.require_builtin))) {
    state_.smoke_property = property_kind;
    return true;
  }
  return false;
}

Status InternalRequireProbe::SmokeTest() {
  Napi::Value realm_exports = CallRequireBuiltin(
      Napi::Function(env_, state_.require_builtin),
      Napi::String::New(env_, "internal/bootstrap/realm"));
  if (realm_exports.IsEmpty()) {
    return Status::Failure(ProbeStatus::kUnsupportedNoGetter,
                           "requireBuiltin('internal/bootstrap/realm') failed");
  }

  if (!realm_exports.IsObject()) {
    return Status::Failure(ProbeStatus::kUnsupportedNoGetter,
                           "internal/bootstrap/realm did not return an object");
  }

  Napi::Object realm_object = realm_exports.As<Napi::Object>();
  if (HasSelfReference(realm_object, "require", SmokePropertyKind::kRequire) ||
      HasSelfReference(
          realm_object, "requireBuiltin", SmokePropertyKind::kRequireBuiltin)) {
    return Status::Ok();
  }

  return Status::Failure(ProbeStatus::kUnsupportedNoGetter,
                         "internal/bootstrap/realm self-id smoke test failed");
}

Result<ProbeOutcome> InternalRequireProbe::LoadTargetModule() {
  Napi::Value target_exports = CallRequireBuiltin(
      Napi::Function(env_, state_.require_builtin), target_id_);
  if (target_exports.IsEmpty()) {
    // A failed target load usually leaves a pending JS exception. Consume it so
    // the public getter can throw one clear unsupported error with diagnostics.
    if (env_.IsExceptionPending()) env_.GetAndClearPendingException();
    state_.status = ProbeStatus::kPartialInternalRequireOnly;
    state_.result = ProbeResultKind::kPartial;
    state_.error = "target internal module load failed";
    return Result<ProbeOutcome>::Ok(ProbeOutcome::kPartial);
  }

  state_.target_exports = target_exports;
  state_.target_exports_ok = true;
  return Result<ProbeOutcome>::Ok(ProbeOutcome::kContinue);
}

Status InternalRequireProbe::RunStatus() {
  if (load_target_ && !IsAllowedTarget(state_.target)) {
    std::string message = "target must be one of: ";
    message.append(AllowedTargetList());
    return Status::Failure(ProbeStatus::kUnsupportedDisallowedTarget, message);
  }

  Status status = ResolveRequireBuiltin();
  if (!status.ok()) return status;
  status = ValidateRequireBuiltinName();
  if (!status.ok()) return status;
  status = SmokeTest();
  if (!status.ok()) return status;
  if (load_target_) {
    auto outcome = LoadTargetModule();
    if (!outcome.ok()) return outcome.status();
    if (outcome.value() == ProbeOutcome::kPartial) {
      return Status::Ok();
    }
  }

  state_.status = ProbeStatus::kSupported;
  state_.result = ProbeResultKind::kSupported;
  state_.error.clear();
  return Status::Ok();
}

bool InternalRequireProbe::Run() {
  Status status = RunStatus();
  if (!status.ok()) {
    state_.status = status.code();
    state_.result = ProbeResultKind::kUnsupported;
    state_.error = status.message();
    diagnostics_ = state_.ToDiagnostics(env_);
    return false;
  }
  diagnostics_ = state_.ToDiagnostics(env_);
  return true;
}

Napi::Value CallRequireBuiltin(Napi::Function require_builtin, Napi::String id) {
  if (require_builtin.IsEmpty() || id.IsEmpty()) return {};
  return require_builtin.Call({id});
}

}  // namespace internal_require
