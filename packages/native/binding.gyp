{
  "variables": {
    "enable_lto": "false",
    "enable_thin_lto": "false",
    "lto_jobs": "",
    "narb_backend%": "napi",
    "narb_product%": "internal-loader"
  },
  "targets": [
    {
      "target_name": "require_builtin",
      "sources": [
        "src/node_api_addon.cc",
        "src/debug_trace.cc",
        "src/native_types.cc",
        "src/runtime_symbol.cc",
        "src/runtime_context/helper.cc",
        "src/runtime_context/platform.cc",
        "src/runtime_context/darwin_arm64.cc",
        "src/runtime_context/darwin_x64.cc",
        "src/runtime_context/linux_glibc_arm64.cc",
        "src/runtime_context/linux_glibc_x64.cc",
        "src/runtime_context/win32_arm64.cc",
        "src/runtime_context/win32_x64.cc",
        "src/runtime_context/win32_ia32.cc",
        "src/runtime_probe/helper.cc",
        "src/runtime_probe/platform.cc",
        "src/runtime_probe/getter_decoder.cc",
        "src/runtime_probe/posix.cc",
        "src/runtime_probe/win32_common.cc",
        "src/runtime_probe/darwin_arm64.cc",
        "src/runtime_probe/darwin_x64.cc",
        "src/runtime_probe/linux_glibc_arm64.cc",
        "src/runtime_probe/linux_glibc_x64.cc",
        "src/runtime_probe/win32_arm64.cc",
        "src/runtime_probe/win32_x64.cc",
        "src/runtime_probe/win32_ia32.cc",
        "src/require_builtin_probe.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_VERSION=9",
        "NODE_ADDON_API_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags_cc": [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-cast-function-type-mismatch",
        "-fno-exceptions",
        "-fvisibility=hidden"
      ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "NO",
        "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
        "WARNING_CFLAGS": [
          "-Wall",
          "-Wextra",
          "-Wno-unused-parameter",
          "-Wno-cast-function-type-mismatch"
        ]
      },
      "conditions": [
        [
          "narb_backend=='nodeabi'",
          {
            "sources": [
              "src/runtime_compat_nodeabi.cc"
            ],
            "include_dirs": [
              "<!@(node -e \"const path=require('node:path'); for (const dir of (process.env.NODE_JS_PUBLIC_INCLUDE_DIRS || '').split(path.delimiter).filter(Boolean)) console.log(dir)\")"
            ],
            "defines": [
              "NARB_BACKEND=2",
              "HAVE_SQLITE=0",
              "HAVE_AMARO=0"
            ],
            "cflags_cc": [
              "-std=c++20"
            ],
            "xcode_settings": {
              "CLANG_CXX_LANGUAGE_STANDARD": "c++20"
            },
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": [
                  "/std:c++20"
                ]
              }
            }
          },
          {
            "sources": [
              "src/runtime_context/runtime_profile.cc",
              "src/runtime_context/runtime_profile_napi.cc",
              "src/runtime_compat_napi.cc"
            ],
            "defines": [
              "NARB_BACKEND=1"
            ],
            "cflags_cc": [
              "-std=c++17"
            ],
            "xcode_settings": {
              "CLANG_CXX_LANGUAGE_STANDARD": "c++17"
            },
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": [
                  "/std:c++17"
                ]
              }
            }
          }
        ],
        [
          "narb_product=='require-builtin'",
          {
            "defines": [
              "NARB_PRODUCT=1"
            ]
          },
          {
            "defines": [
              "NARB_PRODUCT=2"
            ]
          }
        ]
      ]
    }
  ]
}
