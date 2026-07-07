#include "helper.h"

#include <array>
#include <cstring>

namespace internal_require {

void TraceRuntimeContextCodeBytes(const char* label, void* fn) {
  if (!DebugTraceEnabled() || fn == nullptr) return;
  std::array<unsigned char, 24> bytes{};
  std::memcpy(bytes.data(), fn, bytes.size());
  DebugTraceBytes(label, bytes.data(), bytes.size());
}

bool IsEmbedderFieldCountPlausible(uint32_t embedder_fields) {
  // The realm lives in a fixed slot, so the context must expose more than that
  // many fields; an implausibly large count means the pointer we read is not a
  // real v8::Context and we should fail closed rather than trust it.
  return embedder_fields > static_cast<uint32_t>(kRealmSlot) &&
      embedder_fields <= kMaxPlausibleEmbedderFields;
}

Result<CurrentContextRead> ReadDirectCurrentV8Context(
    const char* platform,
    void* isolate,
    const CurrentContextSymbols& symbols) {
  DebugTrace("calling Isolate::GetCurrentContext(isolate) (%s direct)",
              platform);
  void* context = symbols.call_get_current_context(isolate);
  const uintptr_t context_address = reinterpret_cast<uintptr_t>(context);
  DebugTrace("context=%s (%s direct)",
              Hex(context_address).c_str(),
              platform);
  if (!IsPointerAligned(context_address)) {
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "current V8 context is null or unaligned"));
  }

  DebugTrace("calling Context::GetNumberOfEmbedderDataFields(context) (%s direct)",
              platform);
  const uint32_t embedder_fields = symbols.get_fields(context);
  DebugTrace("embedder_fields=%u (realm_slot=%d)",
              embedder_fields, kRealmSlot);
  if (!IsEmbedderFieldCountPlausible(embedder_fields)) {
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "current context embedder field count is implausible"));
  }

  CurrentContextRead read;
  read.context_ptr = context;
  read.embedder_fields = embedder_fields;
  return Result<CurrentContextRead>::Ok(read);
}

}  // namespace internal_require
