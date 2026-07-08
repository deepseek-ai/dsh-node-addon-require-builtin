#include <napi.h>
#include <node_api.h>

#include "require_builtin_probe.h"
#include "native_types.h"

namespace esplus::node::require_builtin {
namespace {

void SetString(Napi::Env env,
               Napi::Object object,
               std::string_view key,
               std::string_view value) {
  object.Set(Napi::String::New(env, key), Napi::String::New(env, value));
}

void SetHex(Napi::Env env, Napi::Object object, std::string_view key, uintptr_t value) {
  if (value == 0) {
    SetString(env, object, key, "");
    return;
  }
  const std::string hex = Hex(value);
  SetString(env, object, key, hex);
}

Napi::Object DiagnosticsObject(Napi::Env env, const Diagnostics& diag) {
  Napi::Object object = Napi::Object::New(env);
  SetString(env, object, "mode", diag.mode);
  SetString(env, object, "backend", diag.backend);
  SetString(env, object, "binary_abi", diag.binary_abi);
  SetString(env, object, "node", diag.node);
  object.Set("napi_version", Napi::Number::New(env, diag.napi_version));
  SetString(env, object, "platform", diag.platform);
  SetString(env, object, "arch", diag.arch);
  object.Set("uses_node_addon_api", Napi::Boolean::New(env, diag.uses_node_addon_api));
  object.Set("has_v8_context", Napi::Boolean::New(env, diag.has_v8_context));
  object.Set("realm_slot", Napi::Number::New(env, kRealmSlot));
  object.Set("embedder_fields", Napi::Number::New(env, diag.embedder_fields));
  SetString(env, object, "embedder_data", diag.embedder_data);
  SetHex(env, object, "isolate", diag.isolate);
  SetHex(env, object, "context", diag.context);
  SetHex(env, object, "realm", diag.realm);
  SetHex(env, object, "realm_vptr", diag.vptr);
  SetHex(env, object, "getter_address", diag.getter);
  SetString(env, object, "getter_symbol", diag.getter_symbol);
  SetString(env, object, "getter_symbol_name", diag.getter_symbol_name);
  SetString(env, object, "getter_pattern", diag.getter_pattern);
  SetString(env, object, "getter_image", diag.getter_image);
  SetString(env, object, "vptr_image", diag.vptr_image);
  SetString(env, object, "offset", diag.offset == 0 ? "" : Hex(diag.offset));
  SetString(env, object, "require_builtin_name", diag.require_builtin_name);
  SetString(env, object, "smoke_property", diag.smoke_property);
  SetString(env, object, "target", diag.target);
  object.Set("target_exports_ok", Napi::Boolean::New(env, diag.target_exports_ok));
  SetString(env, object, "status", diag.status);
  SetString(env, object, "result", diag.result);
  SetString(env, object, "error", diag.error);
  return object;
}

void ThrowUnsupported(Napi::Env env, const RequireBuiltinProbe& probe) {
  const ProbeState& state = probe.state();
  std::string message = "@esplus/node-addon-require-builtin unsupported: ";
  message.append(ProbeStatusString(state.status));
  if (!state.error.empty()) {
    message += " (" + state.error + ")";
  }
  Napi::Error error = Napi::Error::New(env, message);
  error.Set(
      Napi::String::New(env, "code"),
      Napi::String::New(env, ProbeStatusString(state.status)));
  error.Set("diagnostics", DiagnosticsObject(env, probe.diagnostics()));
  error.ThrowAsJavaScriptException();
}

Napi::Value RequireBuiltin(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "moduleId must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  RequireBuiltinProbe probe(env, info[0], true);
  probe.Run();
  const ProbeState& state = probe.state();
  if (state.status != ProbeStatus::kSupported || state.target_exports == nullptr) {
    ThrowUnsupported(env, probe);
    return env.Undefined();
  }

  return Napi::Value(env, state.target_exports);
}

Napi::Value IsAllowedInternalIdBinding(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    return Napi::Boolean::New(env, false);
  }

  const std::string module_id = info[0].As<Napi::String>().Utf8Value();
  return Napi::Boolean::New(env, IsAllowedInternalId(module_id));
}

Napi::Value GetNativeBindingInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object object = Napi::Object::New(env);
  SetString(env, object, "mode", kMode);
  SetString(env, object, "backend", kBackend);
  SetString(env, object, "abi", BinaryAbi());
  return object;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Public Node-API surface. The exported addon remains N-API-loadable across
  // Node versions; private runtime compatibility is handled below the API layer.
  exports.Set("requireBuiltin", Napi::Function::New(env, RequireBuiltin));
  exports.Set("isAllowedInternalId",
              Napi::Function::New(env, IsAllowedInternalIdBinding));
  exports.Set("getNativeBindingInfo", Napi::Function::New(env, GetNativeBindingInfo));
  return exports;
}

}  // namespace
}  // namespace esplus::node::require_builtin

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return esplus::node::require_builtin::Init(env, exports);
}

NODE_API_MODULE(require_builtin, Init)
