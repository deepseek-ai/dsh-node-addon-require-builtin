#include "runtime_compat.h"

#include "backend_config.h"
#include "runtime_context/helper.h"
#include "runtime_context/runtime_profile.h"
#include "runtime_probe/helper.h"

#include <array>
#include <cstring>

#if NARB_BACKEND != NARB_BACKEND_NAPI
#error "runtime_compat_napi.cc only implements NARB_BACKEND_NAPI"
#endif

namespace esplus::node::require_builtin {
namespace {

// Private C++ ABI symbols. These names are not part of Node-API stability;
// they are looked up at runtime so a single N-API binary can reject unsupported
// Node builds instead of being linked to one NODE_MODULE_VERSION.
constexpr std::string_view kSymIsolateGetCurrent =
    "_ZN2v87Isolate10GetCurrentEv";
constexpr std::string_view kSymIsolateGetCurrentContext =
    "_ZN2v87Isolate17GetCurrentContextEv";
constexpr std::string_view kSymContextGetNumberOfEmbedderDataFields =
    "_ZN2v87Context29GetNumberOfEmbedderDataFieldsEv";
constexpr std::string_view kSymContextSlowGetAlignedPointerUntagged =
    "_ZN2v87Context37SlowGetAlignedPointerFromEmbedderDataEi";
constexpr std::string_view kSymContextSlowGetAlignedPointerTagged =
    "_ZN2v87Context37SlowGetAlignedPointerFromEmbedderDataEit";

using GetCurrentIsolateFn = void* (*)();
using SlowGetAlignedPointerUntaggedFn =
    void* (NARB_MEMBER_ABI*)(void*, int);
using SlowGetAlignedPointerTaggedFn =
    void* (NARB_MEMBER_ABI*)(void*, int, uint16_t);

struct EmbedderDataRead {
  void* realm_ptr = nullptr;
  const char* family = nullptr;
};

Result<EmbedderDataRead> ReadRealmFromEmbedderData(
    const NapiRuntimeProfile& profile,
    void* context) {
  if (profile.embedder_data_abi() == EmbedderDataAbi::kTagged) {
    auto get_aligned_tagged =
        LookupProcessFunction<SlowGetAlignedPointerTaggedFn>(
            kSymContextSlowGetAlignedPointerTagged);
    if (get_aligned_tagged == nullptr) {
      return Result<EmbedderDataRead>::Failure(Status::Failure(
          ProbeStatus::kUnsupportedNoRealm,
          "tagged GetAlignedPointerFromEmbedderData symbol not found"));
    }
    DebugTrace("calling SlowGetAlignedPointerFromEmbedderData(tagged)");
    return Result<EmbedderDataRead>::Ok({
        get_aligned_tagged(
            context, kRealmSlot, profile.embedder_data_tag()),
        profile.diagnostic_name(),
    });
  }

  auto get_aligned_untagged =
      LookupProcessFunction<SlowGetAlignedPointerUntaggedFn>(
          kSymContextSlowGetAlignedPointerUntagged);
  if (get_aligned_untagged != nullptr) {
    DebugTrace("calling SlowGetAlignedPointerFromEmbedderData(untagged)");
    return Result<EmbedderDataRead>::Ok({
        get_aligned_untagged(context, kRealmSlot),
        profile.diagnostic_name(),
    });
  }

  return Result<EmbedderDataRead>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoRealm,
      "no compatible GetAlignedPointerFromEmbedderData symbol found"));
}

Result<RuntimeContext> ReadCurrentContext(napi_env env) {
  auto profile = NapiRuntimeProfile::Detect(env);
  if (!profile.ok()) return Result<RuntimeContext>::Failure(profile.status());

  // Public Node-API does not expose v8::Context. This crosses the API boundary
  // by calling exported V8 methods dynamically from the current process.
  auto get_current_isolate =
      LookupProcessFunction<GetCurrentIsolateFn>(kSymIsolateGetCurrent);
  auto get_current_context =
      LookupProcessFunction<GetCurrentContextFn>(kSymIsolateGetCurrentContext);
  auto get_fields =
      LookupProcessFunction<GetNumberOfEmbedderDataFieldsFn>(
          kSymContextGetNumberOfEmbedderDataFields);

  if (get_current_isolate == nullptr ||
      get_current_context == nullptr ||
      get_fields == nullptr) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "required V8 current-context symbols were not found"));
  }
  DebugTrace("symbols: isolate=%p context=%p fields=%p",
              reinterpret_cast<void*>(get_current_isolate),
              reinterpret_cast<void*>(get_current_context),
              reinterpret_cast<void*>(get_fields));
  TraceRuntimeContextCodeBytes(
      "GetCurrent", reinterpret_cast<void*>(get_current_isolate));
  TraceRuntimeContextCodeBytes(
      "GetCurrentContext", reinterpret_cast<void*>(get_current_context));
  TraceRuntimeContextCodeBytes(
      "GetNumberOfEmbedderDataFields", reinterpret_cast<void*>(get_fields));

  RuntimeContext context;
  DebugTrace("calling Isolate::GetCurrent");
  context.isolate_ptr = get_current_isolate();
  context.isolate = reinterpret_cast<uintptr_t>(context.isolate_ptr);
  DebugTrace("isolate=%s", Hex(context.isolate).c_str());
  if (!IsPointerAligned(context.isolate)) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "current isolate is null or unaligned"));
  }

  CurrentContextSymbols context_symbols;
  context_symbols.get_current_context =
      reinterpret_cast<void*>(get_current_context);
  context_symbols.call_get_current_context = get_current_context;
  context_symbols.get_fields = get_fields;

  auto context_read = ReadCurrentV8Context(
      context.isolate_ptr, context_symbols);
  if (!context_read.ok()) {
    return Result<RuntimeContext>::Failure(context_read.status());
  }
  context.context_ptr = context_read.value().context_ptr;
  context.context = reinterpret_cast<uintptr_t>(context.context_ptr);
  context.has_v8_context = true;
  context.embedder_fields = context_read.value().embedder_fields;

  auto embedder_data =
      ReadRealmFromEmbedderData(profile.value(), context.context_ptr);
  if (!embedder_data.ok()) {
    return Result<RuntimeContext>::Failure(embedder_data.status());
  }
  context.embedder_data = embedder_data.value().family;
  context.realm_ptr = embedder_data.value().realm_ptr;

  context.realm = reinterpret_cast<uintptr_t>(context.realm_ptr);
  DebugTrace("realm=%s", Hex(context.realm).c_str());
  if (!IsPointerAligned(context.realm)) {
    return Result<RuntimeContext>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm pointer is null or unaligned"));
  }

  return Result<RuntimeContext>::Ok(context);
}

}  // namespace

Result<RuntimeRequireBuiltin> ProbeRuntimeRequireBuiltin(napi_env env) {
  // Main private-ABI entry point: all Node-version-specific work is contained
  // here so the higher-level requireBuiltin probe can stay version-agnostic.
  DebugTrace("ProbeRuntimeRequireBuiltin: reading current context");
  auto context = ReadCurrentContext(env);
  if (!context.ok()) return Result<RuntimeRequireBuiltin>::Failure(context.status());

  DebugTrace("resolving builtin_module_require getter symbol");
  auto getter = ResolveBuiltinModuleRequireGetter(env, context.value().realm_ptr);
  if (!getter.ok()) return Result<RuntimeRequireBuiltin>::Failure(getter.status());
  DebugTrace("getter=%s", Hex(getter.value().address).c_str());

  auto image = ValidateRuntimeImagePointers(
      context.value().realm_ptr, getter.value().address_ptr);
  if (!image.ok()) return Result<RuntimeRequireBuiltin>::Failure(image.status());

  if (DebugTraceEnabled() && getter.value().address_ptr != nullptr) {
    std::array<unsigned char, 16> bytes{};
    std::memcpy(bytes.data(), getter.value().address_ptr, bytes.size());
    DebugTraceBytes("getter", bytes.data(), bytes.size());
  }

  DebugTrace("parsing getter offset from machine code");
  auto pattern = ParseBuiltinModuleRequireGetterOffset(getter.value().address_ptr);
  if (!pattern.ok()) return Result<RuntimeRequireBuiltin>::Failure(pattern.status());
  DebugTrace("parsed offset=%s pattern=%s",
              Hex(pattern.value().offset).c_str(),
              pattern.value().pattern.c_str());

  DebugTrace("reading and validating requireBuiltin handle from realm field");
  auto require_builtin = ReadAndValidateRequireBuiltinHandle(
      env,
      context.value().realm_ptr, getter.value().address_ptr, pattern.value());
  if (!require_builtin.ok()) {
    return Result<RuntimeRequireBuiltin>::Failure(require_builtin.status());
  }
  DebugTrace("requireBuiltin handle validated");

  RuntimeRequireBuiltin result;
  result.value = require_builtin.value();
  result.context = context.value();
  result.getter = getter.value();
  result.pattern = pattern.value();
  result.image = image.value();
  return Result<RuntimeRequireBuiltin>::Ok(result);
}

}  // namespace esplus::node::require_builtin
