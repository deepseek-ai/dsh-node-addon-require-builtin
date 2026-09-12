#pragma once

#include <string_view>

namespace esplus::node::require_builtin {

void* LookupProcessSymbol(std::string_view name);

template <typename Fn>
Fn LookupProcessFunction(std::string_view name) {
  // Private ABI assumption: the mangled symbol must still have the exact
  // calling convention encoded by Fn for the running Node/V8 build.
  return reinterpret_cast<Fn>(LookupProcessSymbol(name));
}

}  // namespace esplus::node::require_builtin
