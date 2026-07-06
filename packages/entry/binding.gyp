{
  "targets": [
    {
      "target_name": "internal_require",
      "sources": [
        "src/node_api_addon.cc",
        "src/native_types.cc",
        "src/runtime_probe/helper.cc",
        "src/runtime_probe/darwin_arm64.cc",
        "src/runtime_probe/darwin_x64.cc",
        "src/runtime_probe/linux_glibc_arm64.cc",
        "src/runtime_probe/linux_glibc_x64.cc",
        "src/runtime_probe/win32_arm64.cc",
        "src/runtime_probe/win32_x64.cc",
        "src/runtime_compat_napi.cc",
        "src/internal_require_probe.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_VERSION=9",
        "INTERNAL_REQUIRE_BACKEND=1",
        "NODE_ADDON_API_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-cast-function-type-mismatch",
        "-fno-exceptions",
        "-fvisibility=hidden"
      ],
      "xcode_settings": {
        "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
        "GCC_ENABLE_CPP_EXCEPTIONS": "NO",
        "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
        "WARNING_CFLAGS": [
          "-Wall",
          "-Wextra",
          "-Wno-unused-parameter",
          "-Wno-cast-function-type-mismatch"
        ]
      }
    }
  ]
}
