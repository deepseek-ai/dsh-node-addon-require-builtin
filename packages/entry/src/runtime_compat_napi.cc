#include "runtime_compat.h"

#include "backend_config.h"
#include "runtime_context/helper.h"
#include "runtime_probe/helper.h"

#if INTERNAL_REQUIRE_BACKEND != INTERNAL_REQUIRE_BACKEND_NAPI
#error "runtime_compat_napi.cc only implements INTERNAL_REQUIRE_BACKEND_NAPI"
#endif

namespace internal_require {
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
using SlowGetAlignedPointerUntaggedFn = void* (*)(void*, int);
using SlowGetAlignedPointerTaggedFn = void* (*)(void*, int, uint16_t);

enum class EmbedderDataFamily {
  kUntagged,
  kTagged,
};

Result<EmbedderDataFamily> DetectEmbedderDataFamily(napi_env env) {
  const napi_node_version* version = nullptr;
  if (napi_get_node_version(env, &version) != napi_ok || version == nullptr) {
    return Result<EmbedderDataFamily>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext, "could not read Node.js version"));
  }
  return Result<EmbedderDataFamily>::Ok(
      version->major >= 26 ? EmbedderDataFamily::kTagged
                           : EmbedderDataFamily::kUntagged);
}

Result<RuntimeContext> ReadCurrentContext(napi_env env) {
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
  TracePrintf("symbols: isolate=%p context=%p fields=%p",
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
  TracePrintf("calling Isolate::GetCurrent");
  context.isolate_ptr = get_current_isolate();
  context.isolate = reinterpret_cast<uintptr_t>(context.isolate_ptr);
  TracePrintf("isolate=%s", Hex(context.isolate).c_str());
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

  auto family = DetectEmbedderDataFamily(env);
  if (!family.ok()) return Result<RuntimeContext>::Failure(family.status());

  if (family.value() == EmbedderDataFamily::kTagged) {
    // Node 26+ family: V8 requires the per-context embedder-data tag.
    auto get_aligned_tagged =
        LookupProcessFunction<SlowGetAlignedPointerTaggedFn>(
            kSymContextSlowGetAlignedPointerTagged);
    if (get_aligned_tagged == nullptr) {
      return Result<RuntimeContext>::Failure(Status::Failure(
          ProbeStatus::kUnsupportedNoRealm,
          "tagged GetAlignedPointerFromEmbedderData symbol not found"));
    }
    context.embedder_data = "tagged kPerContextData=2";
    TracePrintf("calling SlowGetAlignedPointerFromEmbedderData(tagged)");
    context.realm_ptr = get_aligned_tagged(
        context.context_ptr, kRealmSlot, kPerContextDataTag);
  } else {
    // Node 20/22/24 family: the exported slow path has the older untagged
    // signature. This split is the main runtime version compatibility branch.
    auto get_aligned_untagged =
        LookupProcessFunction<SlowGetAlignedPointerUntaggedFn>(
            kSymContextSlowGetAlignedPointerUntagged);
    if (get_aligned_untagged == nullptr) {
      return Result<RuntimeContext>::Failure(Status::Failure(
          ProbeStatus::kUnsupportedNoRealm,
          "no compatible GetAlignedPointerFromEmbedderData symbol found"));
    }
    context.embedder_data = "untagged Node 20/22/24 family";
    TracePrintf("calling SlowGetAlignedPointerFromEmbedderData(untagged)");
    context.realm_ptr = get_aligned_untagged(context.context_ptr, kRealmSlot);
  }

  context.realm = reinterpret_cast<uintptr_t>(context.realm_ptr);
  TracePrintf("realm=%s", Hex(context.realm).c_str());
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
  TracePrintf("ProbeRuntimeRequireBuiltin: reading current context");
  auto context = ReadCurrentContext(env);
  if (!context.ok()) return Result<RuntimeRequireBuiltin>::Failure(context.status());

  TracePrintf("resolving builtin_module_require getter symbol");
  auto getter = ResolveBuiltinModuleRequireGetter(env, context.value().realm_ptr);
  if (!getter.ok()) return Result<RuntimeRequireBuiltin>::Failure(getter.status());
  TracePrintf("getter=%s", Hex(getter.value().address).c_str());

  auto image = ValidateRuntimeImagePointers(
      context.value().realm_ptr, getter.value().address_ptr);
  if (!image.ok()) return Result<RuntimeRequireBuiltin>::Failure(image.status());

  if (TraceEnabled() && getter.value().address_ptr != nullptr) {
    const auto* bytes =
        static_cast<const unsigned char*>(getter.value().address_ptr);
    TracePrintf(
        "getter bytes: %02x %02x %02x %02x %02x %02x %02x %02x "
        "%02x %02x %02x %02x %02x %02x %02x %02x",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
        bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
        bytes[13], bytes[14], bytes[15]);
  }

  TracePrintf("parsing getter offset from machine code");
  auto pattern = ParseBuiltinModuleRequireGetterOffset(getter.value().address_ptr);
  if (!pattern.ok()) return Result<RuntimeRequireBuiltin>::Failure(pattern.status());
  TracePrintf("parsed offset=%s pattern=%s",
              Hex(pattern.value().offset).c_str(),
              pattern.value().pattern.c_str());

  TracePrintf("reading and validating requireBuiltin handle from realm field");
  auto require_builtin = ReadAndValidateRequireBuiltinHandle(
      env,
      context.value().realm_ptr, getter.value().address_ptr, pattern.value());
  if (!require_builtin.ok()) {
    return Result<RuntimeRequireBuiltin>::Failure(require_builtin.status());
  }
  TracePrintf("requireBuiltin handle validated");

  RuntimeRequireBuiltin result;
  result.value = require_builtin.value();
  result.context = context.value();
  result.getter = getter.value();
  result.pattern = pattern.value();
  result.image = image.value();
  return Result<RuntimeRequireBuiltin>::Ok(result);
}

}  // namespace internal_require
