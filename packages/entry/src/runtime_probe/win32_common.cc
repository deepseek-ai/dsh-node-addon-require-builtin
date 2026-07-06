#if defined(_WIN32)

#include "helper.h"
#include "platform.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

namespace internal_require {
namespace {

constexpr size_t kMaxWindowsRealmVtableScanSlots = 192;
constexpr size_t kMaxWindowsVtableCandidateTrace = 128;
constexpr size_t kMinWindowsPerRealmGetterRunLength = 32;
constexpr std::string_view kRequireBuiltinName = "requireBuiltin";

struct WindowsSymbolAlias {
  std::string_view itanium;
  std::string_view msvc;
};

constexpr std::array<WindowsSymbolAlias, 5> kWindowsSymbolAliases = {{
    {"_ZN2v87Isolate10GetCurrentEv",
     "?GetCurrent@Isolate@v8@@SAPEAV12@XZ"},
    {"_ZN2v87Isolate17GetCurrentContextEv",
     "?GetCurrentContext@Isolate@v8@@QEAA?AV?$Local@VContext@v8@@@2@XZ"},
    {"_ZN2v87Context29GetNumberOfEmbedderDataFieldsEv",
     "?GetNumberOfEmbedderDataFields@Context@v8@@QEAAIXZ"},
    {"_ZN2v87Context37SlowGetAlignedPointerFromEmbedderDataEi",
     "?SlowGetAlignedPointerFromEmbedderData@Context@v8@@AEAAPEAXH@Z"},
    {"_ZN2v87Context37SlowGetAlignedPointerFromEmbedderDataEit",
     "?SlowGetAlignedPointerFromEmbedderData@Context@v8@@AEAAPEAXHG@Z"},
}};

struct WindowsAddressInfo {
  HMODULE module = nullptr;
  uintptr_t region_start = 0;
  uintptr_t region_end = 0;
  DWORD protect = 0;
  std::string module_path;
};

struct WindowsVtableScanStats {
  size_t unaligned = 0;
  size_t address_query_failed = 0;
  size_t different_module = 0;
  size_t unreadable = 0;
  size_t non_executable = 0;
  size_t executable_candidates = 0;
  size_t parse_failed = 0;
  size_t pattern_matched = 0;
};

struct WindowsVtableGetter {
  size_t slot = 0;
  void* address = nullptr;
  GetterPattern pattern;
};

std::string_view WindowsMsvcSymbolName(std::string_view name) {
  for (const auto& alias : kWindowsSymbolAliases) {
    if (alias.itanium == name) return alias.msvc;
  }
  return {};
}

HMODULE LookupWindowsNodeModule(std::wstring_view name) {
  if (name.empty()) return GetModuleHandleW(nullptr);
  return GetModuleHandleW(name.data());
}

DWORD WindowsProtectionBase(DWORD protect) {
  return protect & 0xff;
}

bool IsWindowsReadableProtection(DWORD protect) {
  switch (WindowsProtectionBase(protect)) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
      return true;
    default:
      return false;
  }
}

bool IsWindowsExecutableProtection(DWORD protect) {
  switch (WindowsProtectionBase(protect)) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
      return true;
    default:
      return false;
  }
}

std::string Utf8FromWide(std::wstring_view input) {
  if (input.empty()) return {};
  const int required = WideCharToMultiByte(
      CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
      nullptr, 0, nullptr, nullptr);
  if (required <= 0) return {};
  std::string output(static_cast<size_t>(required), '\0');
  const int written = WideCharToMultiByte(
      CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
      output.data(), required, nullptr, nullptr);
  if (written != required) return {};
  return output;
}

std::string WindowsModuleLookupName(std::wstring_view name) {
  if (name.empty()) return "<process>";
  return Utf8FromWide(name);
}

std::string WindowsModulePath(HMODULE module) {
  if (module == nullptr) return {};

  std::vector<wchar_t> buffer(512);
  for (;;) {
    const DWORD length = GetModuleFileNameW(
        module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size()) {
      return Utf8FromWide(
          std::wstring_view(buffer.data(), static_cast<size_t>(length)));
    }
    if (buffer.size() >= 32768) return {};
    buffer.resize(buffer.size() * 2);
  }
}

bool QueryWindowsAddressInfo(void* address, WindowsAddressInfo* info) {
  if (address == nullptr || info == nullptr) return false;
  MEMORY_BASIC_INFORMATION mbi;
  std::memset(&mbi, 0, sizeof(mbi));
  if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0) return false;
  info->module = static_cast<HMODULE>(mbi.AllocationBase);
  info->region_start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
  info->region_end = info->region_start + mbi.RegionSize;
  info->protect = mbi.Protect;
  info->module_path = WindowsModulePath(info->module);
  return true;
}

bool IsWindowsReadableRange(const void* address, size_t size) {
  WindowsAddressInfo info;
  if (!QueryWindowsAddressInfo(const_cast<void*>(address), &info) ||
      !IsWindowsReadableProtection(info.protect)) {
    return false;
  }

  const uintptr_t start = reinterpret_cast<uintptr_t>(address);
  return start >= info.region_start &&
      start < info.region_end &&
      size <= info.region_end - start;
}

void TraceWindowsAddressInfo(const char* label,
                             void* address,
                             const WindowsAddressInfo& info) {
  DebugTrace(
      "%s address=%s module=%s region=[%s,%s) protect=0x%lx path=%s",
      label,
      Hex(reinterpret_cast<uintptr_t>(address)).c_str(),
      Hex(reinterpret_cast<uintptr_t>(info.module)).c_str(),
      Hex(info.region_start).c_str(),
      Hex(info.region_end).c_str(),
      static_cast<unsigned long>(info.protect),
      info.module_path.empty() ? "<unknown>" : info.module_path.c_str());
}

void TraceWindowsCodeBytes(const char* label, size_t slot, void* fn) {
  if (!DebugTraceEnabled() || fn == nullptr) return;
  WindowsAddressInfo info;
  const uintptr_t fn_address = reinterpret_cast<uintptr_t>(fn);
  if (!QueryWindowsAddressInfo(fn, &info) ||
      fn_address < info.region_start ||
      fn_address > info.region_end ||
      info.region_end - fn_address < 16) {
    DebugTrace("%s slot=%zu fn=%s bytes=<unavailable>",
                label,
                slot,
                Hex(fn_address).c_str());
    return;
  }
  std::array<unsigned char, 16> bytes{};
  std::memcpy(bytes.data(), fn, bytes.size());
  char scratch[64];
  std::snprintf(scratch, sizeof(scratch), "%s slot=%zu fn=%s",
                label, slot, Hex(fn_address).c_str());
  DebugTraceBytes(scratch, bytes.data(), bytes.size());
}

void TraceWindowsVtableScanSummary(const WindowsVtableScanStats& stats) {
  DebugTrace(
      "win32 vtable scan summary: unaligned=%zu address_query_failed=%zu "
      "different_module=%zu unreadable=%zu non_executable=%zu "
      "executable_candidates=%zu parse_failed=%zu pattern_matched=%zu",
      stats.unaligned,
      stats.address_query_failed,
      stats.different_module,
      stats.unreadable,
      stats.non_executable,
      stats.executable_candidates,
      stats.parse_failed,
      stats.pattern_matched);
}

bool ResolveExecutableVtableCandidate(void* candidate,
                                      HMODULE expected_module,
                                      WindowsVtableScanStats* stats,
                                      WindowsAddressInfo* candidate_info) {
  if (!IsPointerAligned(reinterpret_cast<uintptr_t>(candidate))) {
    stats->unaligned++;
    return false;
  }

  if (!QueryWindowsAddressInfo(candidate, candidate_info)) {
    stats->address_query_failed++;
    return false;
  }
  if (candidate_info->module != expected_module) {
    stats->different_module++;
    return false;
  }
  if (!IsWindowsReadableProtection(candidate_info->protect)) {
    stats->unreadable++;
    return false;
  }
  if (!IsWindowsExecutableProtection(candidate_info->protect)) {
    stats->non_executable++;
    return false;
  }

  stats->executable_candidates++;
  return true;
}

const WindowsVtableGetter* FindGetterAtSlot(
    const std::vector<WindowsVtableGetter>& getters,
    size_t slot) {
  for (const auto& getter : getters) {
    if (getter.slot == slot) return &getter;
  }
  return nullptr;
}

size_t ConsecutivePerRealmGetterRunLength(
    const std::vector<WindowsVtableGetter>& getters,
    const WindowsVtableGetter& start) {
  size_t length = 0;
  for (;;) {
    const size_t expected_slot = start.slot + length * 2;
    const size_t expected_offset =
        start.pattern.offset + length * sizeof(void*);
    const WindowsVtableGetter* getter = FindGetterAtSlot(getters, expected_slot);
    if (getter == nullptr || getter->pattern.offset != expected_offset) break;
    length++;
  }
  return length;
}

bool NapiFunctionNameEquals(napi_env env,
                            napi_value function,
                            std::string_view expected) {
  napi_value name = nullptr;
  if (napi_get_named_property(env, function, "name", &name) != napi_ok ||
      name == nullptr) {
    return false;
  }

  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, name, &type) != napi_ok || type != napi_string) {
    return false;
  }

  size_t size = 0;
  if (napi_get_value_string_utf8(env, name, nullptr, 0, &size) != napi_ok) {
    return false;
  }

  std::string actual(size + 1, '\0');
  if (napi_get_value_string_utf8(
          env, name, actual.data(), actual.size(), &size) != napi_ok) {
    return false;
  }
  actual.resize(size);
  DebugTrace("win32 vtable field function name=%s expected=%.*s",
              actual.c_str(),
              static_cast<int>(expected.size()),
              expected.data());
  return actual == expected;
}

const WindowsVtableGetter* ResolveBuiltinModuleRequireGetterFromParsedVtable(
    napi_env env,
    void* realm,
    const std::vector<WindowsVtableGetter>& getters) {
  const WindowsVtableGetter* best_start = nullptr;
  size_t best_length = 0;

  for (const auto& getter : getters) {
    const size_t length = ConsecutivePerRealmGetterRunLength(getters, getter);
    if (length > best_length) {
      best_start = &getter;
      best_length = length;
    }
  }

  if (best_start == nullptr ||
      best_length < kMinWindowsPerRealmGetterRunLength) {
    DebugTrace(
        "win32 vtable per-realm getter run too short: best_length=%zu "
        "min_length=%zu",
        best_length,
        kMinWindowsPerRealmGetterRunLength);
    return nullptr;
  }

  DebugTrace(
      "win32 vtable per-realm getter run: start_slot=%zu start_offset=%s "
      "length=%zu",
      best_start->slot,
      Hex(best_start->pattern.offset).c_str(),
      best_length);

  for (size_t index = 0; index < best_length; ++index) {
    const size_t slot = best_start->slot + index * 2;
    const WindowsVtableGetter* getter = FindGetterAtSlot(getters, slot);
    if (getter == nullptr) continue;

    napi_value candidate = nullptr;
    const void* field =
        static_cast<const uint8_t*>(realm) + getter->pattern.offset;
    if (!IsWindowsReadableRange(field, sizeof(candidate))) {
      DebugTrace(
          "win32 vtable field candidate is not readable: index=%zu slot=%zu "
          "offset=%s",
          index,
          slot,
          Hex(getter->pattern.offset).c_str());
      continue;
    }
    std::memcpy(&candidate, field, sizeof(candidate));
    DebugTrace(
        "win32 vtable field candidate index=%zu slot=%zu offset=%s value=%s",
        index,
        slot,
        Hex(getter->pattern.offset).c_str(),
        Hex(reinterpret_cast<uintptr_t>(candidate)).c_str());
    if (candidate == nullptr) continue;

    napi_valuetype type = napi_undefined;
    if (napi_typeof(env, candidate, &type) != napi_ok ||
        type != napi_function) {
      continue;
    }
    if (NapiFunctionNameEquals(env, candidate, kRequireBuiltinName)) {
      DebugTrace(
          "win32 vtable selected requireBuiltin getter: index=%zu slot=%zu "
          "offset=%s",
          index,
          slot,
          Hex(getter->pattern.offset).c_str());
      return getter;
    }
  }

  DebugTrace("win32 vtable per-realm getter run did not contain requireBuiltin");
  return nullptr;
}

}  // namespace

void* LookupPlatformProcessSymbol(std::string_view name) {
  const std::array<std::wstring_view, 3> modules = {
      std::wstring_view{},
      std::wstring_view{L"node.exe"},
      std::wstring_view{L"node.dll"},
  };

  for (const auto& module_name : modules) {
    HMODULE module = LookupWindowsNodeModule(module_name);
    const std::string module_lookup_name = WindowsModuleLookupName(module_name);
    if (module == nullptr) {
      DebugTrace("GetModuleHandleW(%s) -> null", module_lookup_name.c_str());
      continue;
    }
    const std::string module_path = WindowsModulePath(module);
    DebugTrace("GetModuleHandleW(%s) -> %s path=%s",
                module_lookup_name.c_str(),
                Hex(reinterpret_cast<uintptr_t>(module)).c_str(),
                module_path.empty() ? "<unknown>" : module_path.c_str());

    if (FARPROC symbol = GetProcAddress(module, name.data())) {
      DebugTrace("GetProcAddress(%s, %.*s) -> %s",
                  module_lookup_name.c_str(),
                  static_cast<int>(name.size()),
                  name.data(),
                  Hex(reinterpret_cast<uintptr_t>(symbol)).c_str());
      return reinterpret_cast<void*>(symbol);
    }
    DebugTrace("GetProcAddress(%s, %.*s) -> null",
                module_lookup_name.c_str(),
                static_cast<int>(name.size()),
                name.data());

    const std::string_view alias = WindowsMsvcSymbolName(name);
    if (!alias.empty()) {
      if (FARPROC symbol = GetProcAddress(module, alias.data())) {
        DebugTrace("GetProcAddress(%s, %.*s) -> %s",
                    module_lookup_name.c_str(),
                    static_cast<int>(alias.size()),
                    alias.data(),
                    Hex(reinterpret_cast<uintptr_t>(symbol)).c_str());
        return reinterpret_cast<void*>(symbol);
      }
      DebugTrace("GetProcAddress(%s, %.*s) -> null",
                  module_lookup_name.c_str(),
                  static_cast<int>(alias.size()),
                  alias.data());
    }
  }
  return nullptr;
}

Result<GetterSymbol> ResolvePlatformBuiltinModuleRequireGetterFallback(
    napi_env env,
    void* realm) {
  auto vptr = ReadRealmVptr(realm);
  if (!vptr.ok()) {
    DebugTrace("win32 vtable fallback rejected realm before vptr read: %s",
                vptr.status().message().c_str());
    return Result<GetterSymbol>::Failure(vptr.status());
  }
  DebugTrace("win32 vtable fallback: realm=%s vptr=%s",
              Hex(reinterpret_cast<uintptr_t>(realm)).c_str(),
              Hex(reinterpret_cast<uintptr_t>(vptr.value())).c_str());

  WindowsAddressInfo vptr_info;
  if (!QueryWindowsAddressInfo(vptr.value(), &vptr_info) ||
      vptr_info.module == nullptr ||
      !IsWindowsReadableProtection(vptr_info.protect)) {
    DebugTrace("win32 vtable fallback rejected vptr image/protection");
    return Result<GetterSymbol>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "realm vptr is not in a readable loaded image"));
  }
  TraceWindowsAddressInfo("win32 realm vptr image", vptr.value(), vptr_info);

  const uintptr_t table_address = reinterpret_cast<uintptr_t>(vptr.value());
  if (table_address < vptr_info.region_start ||
      table_address >= vptr_info.region_end) {
    return Result<GetterSymbol>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "realm vptr does not point into a readable image region"));
  }

  const size_t readable_slots = std::min(
      kMaxWindowsRealmVtableScanSlots,
      (vptr_info.region_end - table_address) / sizeof(void*));
  const auto* table = static_cast<void* const*>(vptr.value());
  DebugTrace(
      "scanning %zu realm vtable slots for builtin_module_require getter "
      "(table=%s)",
      readable_slots,
      Hex(table_address).c_str());

  WindowsVtableScanStats stats;
  std::vector<WindowsVtableGetter> parsed_getters;
  // Keep vtable scanning side-effect free. Candidate functions are not called
  // here; only their code bytes and the matching Realm fields are inspected.
  for (size_t slot = 0; slot < readable_slots; ++slot) {
    void* candidate = table[slot];
    WindowsAddressInfo candidate_info;
    if (!ResolveExecutableVtableCandidate(
            candidate, vptr_info.module, &stats, &candidate_info)) {
      continue;
    }

    if (stats.executable_candidates <= kMaxWindowsVtableCandidateTrace) {
      TraceWindowsCodeBytes("win32 vtable executable candidate", slot, candidate);
    }

    auto pattern = ParseBuiltinModuleRequireGetterOffset(candidate);
    if (!pattern.ok()) {
      stats.parse_failed++;
      continue;
    }
    stats.pattern_matched++;
    DebugTrace("win32 vtable slot %zu pattern matched: offset=%s pattern=%s",
                slot,
                Hex(pattern.value().offset).c_str(),
                pattern.value().pattern.c_str());
    parsed_getters.push_back(
        WindowsVtableGetter{slot, candidate, pattern.value()});
  }

  const WindowsVtableGetter* target =
      ResolveBuiltinModuleRequireGetterFromParsedVtable(
          env, realm, parsed_getters);
  if (target != nullptr) {
    TraceWindowsVtableScanSummary(stats);
    DebugTrace(
        "resolved builtin_module_require getter from per-realm vtable sequence "
        "slot=%zu address=%s offset=%s pattern=%s",
        target->slot,
        Hex(reinterpret_cast<uintptr_t>(target->address)).c_str(),
        Hex(target->pattern.offset).c_str(),
        target->pattern.pattern.c_str());
    GetterSymbol symbol;
    symbol.address_ptr = target->address;
    symbol.address = reinterpret_cast<uintptr_t>(target->address);
    symbol.symbol = "realm-vtable-sequence";
    symbol.symbol_name = "PrincipalRealm::builtin_module_require[vtable]";
    return Result<GetterSymbol>::Ok(symbol);
  }

  TraceWindowsVtableScanSummary(stats);
  return Result<GetterSymbol>::Failure(Status::Failure(
      ProbeStatus::kUnsupportedNoGetter,
      "PrincipalRealm::builtin_module_require getter not exported and no matching vtable getter was found"));
}

Result<ImageValidation> ValidatePlatformRuntimeImagePointers(void* getter,
                                                             void* realm_vptr) {
  ImageValidation image;
  image.vptr = reinterpret_cast<uintptr_t>(realm_vptr);

  WindowsAddressInfo getter_info;
  WindowsAddressInfo vptr_info;
  if (!QueryWindowsAddressInfo(getter, &getter_info) ||
      getter_info.module == nullptr ||
      !IsWindowsExecutableProtection(getter_info.protect)) {
    DebugTrace("win32 image validation rejected getter address=%s",
                Hex(reinterpret_cast<uintptr_t>(getter)).c_str());
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoGetter,
        "getter address is not in an executable loaded image"));
  }
  if (!QueryWindowsAddressInfo(realm_vptr, &vptr_info) ||
      vptr_info.module == nullptr ||
      !IsWindowsReadableProtection(vptr_info.protect)) {
    DebugTrace("win32 image validation rejected realm vptr=%s",
                Hex(reinterpret_cast<uintptr_t>(realm_vptr)).c_str());
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr is not in a readable loaded image"));
  }
  TraceWindowsAddressInfo("win32 getter image", getter, getter_info);
  TraceWindowsAddressInfo("win32 realm vptr image", realm_vptr, vptr_info);

  image.getter_image = getter_info.module_path;
  image.vptr_image = vptr_info.module_path;
  if (getter_info.module != vptr_info.module) {
    DebugTrace("win32 image validation module mismatch: getter_module=%s vptr_module=%s",
                Hex(reinterpret_cast<uintptr_t>(getter_info.module)).c_str(),
                Hex(reinterpret_cast<uintptr_t>(vptr_info.module)).c_str());
    return Result<ImageValidation>::Failure(Status::Failure(
        ProbeStatus::kUnsupportedNoRealm,
        "realm vptr image does not match getter image"));
  }
  DebugTrace("win32 image validation ok: getter and vptr module match");
  return Result<ImageValidation>::Ok(image);
}

}  // namespace internal_require

#endif  // defined(_WIN32)
