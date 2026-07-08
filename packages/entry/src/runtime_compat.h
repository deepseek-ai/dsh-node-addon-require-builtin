#pragma once

#include "native_types.h"

namespace esplus::node::require_builtin {

struct RuntimeContext {
  void* isolate_ptr = nullptr;
  void* context_ptr = nullptr;
  void* realm_ptr = nullptr;
  uintptr_t isolate = 0;
  uintptr_t context = 0;
  uintptr_t realm = 0;
  uint32_t embedder_fields = 0;
  std::string embedder_data;
  bool has_v8_context = false;
};

struct GetterSymbol {
  void* address_ptr = nullptr;
  uintptr_t address = 0;
  std::string symbol = "not-found";
  std::string symbol_name;
};

enum class GetterCallMode {
  kDirectReturn,
  kSret,
};

struct GetterPattern {
  size_t offset = 0;
  std::string pattern = "none";
  GetterCallMode call_mode = GetterCallMode::kDirectReturn;
};

struct ImageValidation {
  uintptr_t vptr = 0;
  std::string getter_image;
  std::string vptr_image;
};

struct RuntimeRequireBuiltin {
  napi_value value = nullptr;
  RuntimeContext context;
  GetterSymbol getter;
  GetterPattern pattern;
  ImageValidation image;
};

// Non-public boundary: this keeps the addon loadable through Node-API, but it
// deliberately probes private Node/V8 runtime state. It must fail closed when
// symbols, layouts, or handle representations do not match the current process.
Result<RuntimeRequireBuiltin> ProbeRuntimeRequireBuiltin(napi_env env);

}  // namespace esplus::node::require_builtin
