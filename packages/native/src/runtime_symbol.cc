#include "runtime_symbol.h"

#include "runtime_probe/platform.h"

namespace esplus::node::require_builtin {

void* LookupProcessSymbol(std::string_view name) {
  return LookupPlatformProcessSymbol(name);
}

}  // namespace esplus::node::require_builtin
