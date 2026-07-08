#include "runtime_compat.h"

#include "backend_config.h"
#include "runtime_context/helper.h"
#include "runtime_probe/helper.h"

#if NARB_BACKEND == NARB_BACKEND_NODEABI || \
    NARB_BACKEND == NARB_BACKEND_NODEABI_VERIFY

#include <node.h>
#include <node_version.h>
#include <v8.h>

namespace esplus::node::require_builtin {
namespace {

static_assert(sizeof(v8::Local<v8::Value>) == sizeof(napi_value),
              "napi_value and v8::Local<Value> handle representations differ");

void* ReadRealmFromEmbedderData(v8::Isolate* isolate,
                                v8::Local<v8::Context> context) {
#if NODE_MAJOR_VERSION >= 26
  // Public V8 API, private Node convention: Node stores per-context data in
  // slot 38 and V8 14+ requires the external-pointer type tag used for it.
  return context->GetAlignedPointerFromEmbedderData(
      isolate,
      kRealmSlot,
      static_cast<v8::EmbedderDataTypeTag>(kPerContextDataTag));
#elif NODE_MAJOR_VERSION >= 24
  // Node 24 public V8 headers expose the isolate-aware overload, but the slot
  // is still untagged.
  return context->GetAlignedPointerFromEmbedderData(isolate, kRealmSlot);
#else
  return context->GetAlignedPointerFromEmbedderData(kRealmSlot);
#endif
}

std::string_view EmbedderDataMode() {
#if NODE_MAJOR_VERSION >= 26
  return "public v8 tagged kPerContextData=2";
#elif NODE_MAJOR_VERSION >= 24
  return "public v8 untagged isolate overload";
#else
  return "public v8 untagged";
#endif
}

Result<RuntimeContext> ReadCurrentContext() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "v8::Isolate::GetCurrent() returned null"));
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
  if (v8_context.IsEmpty()) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext, "current V8 context is empty"));
  }

  RuntimeContext context;
  context.isolate_ptr = isolate;
  context.context_ptr = *v8_context;
  context.isolate = reinterpret_cast<uintptr_t>(context.isolate_ptr);
  context.context = reinterpret_cast<uintptr_t>(context.context_ptr);
  context.has_v8_context = IsPointerAligned(context.context);
  if (!IsPointerAligned(context.isolate) || !context.has_v8_context) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "current isolate or context is unaligned"));
  }

  context.embedder_fields = v8_context->GetNumberOfEmbedderDataFields();
  if (!IsEmbedderFieldCountPlausible(context.embedder_fields)) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "current context embedder field count is implausible"));
  }

  context.embedder_data = EmbedderDataMode();
  context.realm_ptr = ReadRealmFromEmbedderData(isolate, v8_context);
  context.realm = reinterpret_cast<uintptr_t>(context.realm_ptr);
  if (!IsPointerAligned(context.realm)) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm pointer is null or unaligned"));
  }

  return Result<RuntimeContext>::Ok(context);
}

}  // namespace

Result<RuntimeRequireBuiltin> ProbeRuntimeRequireBuiltin(napi_env env) {
  // nodeabi is ABI-specific because it compiles against public Node/V8 headers,
  // but it still resolves Node private state dynamically and validates it.
  auto context = ReadCurrentContext();
  if (!context.ok()) return Result<RuntimeRequireBuiltin>::Failure(context.status());

  auto getter = ResolveBuiltinModuleRequireGetter(env, context.value().realm_ptr);
  if (!getter.ok()) return Result<RuntimeRequireBuiltin>::Failure(getter.status());

  auto image = ValidateRuntimeImagePointers(
      context.value().realm_ptr, getter.value().address_ptr);
  if (!image.ok()) return Result<RuntimeRequireBuiltin>::Failure(image.status());

  auto pattern = ParseBuiltinModuleRequireGetterOffset(getter.value().address_ptr);
  if (!pattern.ok()) return Result<RuntimeRequireBuiltin>::Failure(pattern.status());

  auto require_builtin = ReadAndValidateRequireBuiltinHandle(
      env,
      context.value().realm_ptr, getter.value().address_ptr, pattern.value());
  if (!require_builtin.ok()) {
    return Result<RuntimeRequireBuiltin>::Failure(require_builtin.status());
  }

  RuntimeRequireBuiltin result;
  result.value = require_builtin.value();
  result.context = context.value();
  result.getter = getter.value();
  result.pattern = pattern.value();
  result.image = image.value();
  return Result<RuntimeRequireBuiltin>::Ok(result);
}

}  // namespace esplus::node::require_builtin

#endif
