#include "helper.h"

#include <cstring>

namespace internal_require {

void TraceRuntimeContextCodeBytes(const char* label, void* fn) {
  if (!TraceEnabled() || fn == nullptr) return;
  unsigned char bytes[24] = {0};
  std::memcpy(bytes, fn, sizeof(bytes));
  TracePrintf(
      "%s bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
      "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
      label, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
      bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
      bytes[13], bytes[14], bytes[15], bytes[16], bytes[17], bytes[18],
      bytes[19], bytes[20], bytes[21], bytes[22], bytes[23]);
}

Result<CurrentContextRead> ReadDirectCurrentV8Context(
    const char* platform,
    void* isolate,
    const CurrentContextSymbols& symbols) {
  TracePrintf("calling Isolate::GetCurrentContext(isolate) (%s direct)",
              platform);
  void* context = symbols.call_get_current_context(isolate);
  const uintptr_t context_address = reinterpret_cast<uintptr_t>(context);
  TracePrintf("context=%s (%s direct)",
              Hex(context_address).c_str(),
              platform);
  if (!IsPointerAligned(context_address)) {
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoContext,
        "current V8 context is null or unaligned"));
  }

  TracePrintf("calling Context::GetNumberOfEmbedderDataFields(context) (%s direct)",
              platform);
  const uint32_t embedder_fields = symbols.get_fields(context);
  TracePrintf("embedder_fields=%u (realm_slot=%d)",
              embedder_fields, kRealmSlot);
  if (embedder_fields <= static_cast<uint32_t>(kRealmSlot)) {
    return Result<CurrentContextRead>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "current context has too few embedder data fields"));
  }

  CurrentContextRead read;
  read.context_ptr = context;
  read.embedder_fields = embedder_fields;
  return Result<CurrentContextRead>::Ok(read);
}

}  // namespace internal_require
