#include <napi.h>
#include <node_api.h>

#include "internal_require_probe.h"
#include "native_types.h"

namespace internal_require {
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

void ThrowUnsupported(Napi::Env env, const InternalRequireProbe& probe) {
  const ProbeState& state = probe.state();
  std::string message = "@deepseek-ai/dsh-node-addon-internal unsupported: ";
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

Napi::Value GetLoader(Napi::Env env, std::string_view target) {
  InternalRequireProbe probe(env, Napi::String::New(env, target), true);
  probe.Run();
  const ProbeState& state = probe.state();
  if (state.status != ProbeStatus::kSupported || state.target_exports == nullptr) {
    ThrowUnsupported(env, probe);
    return env.Undefined();
  }

  return Napi::Value(env, state.target_exports);
}

Napi::Value GetModulesCjsLoader(const Napi::CallbackInfo& info) {
  return GetLoader(info.Env(), kCjsLoaderTarget);
}

Napi::Value GetModulesEsmLoader(const Napi::CallbackInfo& info) {
  return GetLoader(info.Env(), kEsmLoaderTarget);
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
  exports.Set("getModulesCjsLoader", Napi::Function::New(env, GetModulesCjsLoader));
  exports.Set("getModulesEsmLoader", Napi::Function::New(env, GetModulesEsmLoader));
  exports.Set("getNativeBindingInfo", Napi::Function::New(env, GetNativeBindingInfo));
  return exports;
}

}  // namespace
}  // namespace internal_require

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return internal_require::Init(env, exports);
}

NODE_API_MODULE(internal_require, Init)
