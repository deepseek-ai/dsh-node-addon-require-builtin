# Node addon internal require

[English](README.md) | 中文

这些 Node-API 原生插件可以获取 Node 内部的 `requireBuiltin()`，无需在启动 Node 时添加
`--expose-internals`。

它们并非完全基于公开 Node-API 的实现。每个插件都通过稳定的 N-API v9 入口加载，
但核心功能依赖于探测 Node/V8 的私有运行时状态，并在运行时验证结果。
如果运行时不满足预期的不变条件，插件必须拒绝继续运行，而不是猜测偏移量。

## 安装

```sh
npm install node-addon-internal-loader
# or
npm install node-addon-require-builtin
```

发布的包分为两个产品系列，以及一个共享的加载器包：

```text
node-addon-native-custom-loader
node-addon-require-builtin
node-addon-require-builtin-darwin-arm64
node-addon-require-builtin-darwin-x64
node-addon-require-builtin-linux-arm64-gnu
node-addon-require-builtin-linux-x64-gnu
node-addon-require-builtin-win32-arm64-msvc
node-addon-require-builtin-win32-ia32-msvc
node-addon-require-builtin-win32-x64-msvc
node-addon-internal-loader
node-addon-internal-loader-darwin-arm64
node-addon-internal-loader-darwin-x64
node-addon-internal-loader-linux-arm64-gnu
node-addon-internal-loader-linux-x64-gnu
node-addon-internal-loader-win32-arm64-msvc
node-addon-internal-loader-win32-ia32-msvc
node-addon-internal-loader-win32-x64-msvc
```

每个入口包都需要一个经过 CI 验证、适用于当前平台的可选依赖包。
如果没有兼容的可选预构建包，安装将直接失败，而不是编译未经验证的本地二进制文件。

## 用法

如果只需要访问 CommonJS 和 ESM 加载器的内部实现，请使用 `node-addon-internal-loader`：

```js
const internalLoader = require('node-addon-internal-loader');

const esmLoader = internalLoader.requireBuiltin('internal/modules/esm/loader');
const cascadedLoader = esmLoader.getOrInitializeCascadedLoader();
```

如果需要不受限制地加载内部内置模块，请使用 `node-addon-require-builtin`：

```js
const requireBuiltinAddon = require('node-addon-require-builtin');

const realm = requireBuiltinAddon.requireBuiltin('internal/bootstrap/realm');
```

两个入口包都提供相同的精简 API：

- `requireBuiltin(moduleId)`：返回指定的内部模块。
- `isAllowedInternalId(moduleId)`：返回是否允许加载 `moduleId`。
- `getBindingInfo()`：按需返回原生绑定的诊断信息，例如 `mode`、
  `product`、`backend`、`abi`、`bindingSource` 和 `bindingPath`。

`node-addon-internal-loader` 只允许加载 `internal/modules/cjs/loader` 和
`internal/modules/esm/loader`；原生插件会在调用 Node 内部的 `requireBuiltin()`
之前执行白名单检查。`node-addon-require-builtin` 不限制模块 ID：
`isAllowedInternalId()` 始终返回 `true`，而 `requireBuiltin(id)` 会将任意字符串 ID
转交给 Node。
白名单和支持的 Node 版本范围请参阅 [docs/internal-modules.md](docs/internal-modules.md)。

返回的内部模块属于 Node 不稳定的实现细节。这些包不会将 Node 内部实现变成公开 API。

## 后端

原生实现包含产品和后端两个维度：

产品：

- `internal-loader`：默认产品，执行 CJS/ESM 加载器白名单检查。
- `require-builtin`：不受限制的产品，将任意字符串 ID 转交给 Node 的 builtin require。

后端：

- `napi`：默认的发布后端。为每个受支持的平台/架构构建一个 `napi-v9` 二进制文件，
  并在运行时探测 Node/V8 的私有状态。
- `nodeabi`：按 Node 主版本/ABI 区分的后端，用于本仓库的源码构建验证，
  不作为可选预构建产物发布。

发布的加载器默认使用 `auto`，会选择当前平台的 `napi-v9` 可选预构建包。
设置 `NARB_BACKEND=napi` 可强制使用该后端。源码构建和特定产品的测试通过
`NARB_PRODUCT=internal-loader` 或 `NARB_PRODUCT=require-builtin` 选择产品。

## 开发

```sh
corepack enable
pnpm install
pnpm build
pnpm test
```

`pnpm build` 会构建 TypeScript 入口和 N-API 原生插件，默认将原生插件输出到
`packages/internal-loader/entry/build/`。设置 `NARB_PRODUCT=require-builtin`
可将不受限制的产品构建到 `packages/require-builtin/entry/build/`。
源码构建仅用于仓库开发和 CI 流程；发布的包不包含用于安装时回退构建的原生源码。

使用官方 Node.js 公共头文件比较后端：

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

准备当前平台的预构建产物：

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm build:prebuild:napi
pnpm test:optional
```

完整的本地开发流程请参阅 [docs/development.md](docs/development.md)，
运行时设计请参阅 [docs/architecture.md](docs/architecture.md)，
可选依赖包模型请参阅 [docs/packaging.md](docs/packaging.md)。

## 支持状态

可选预构建包所支持的平台范围有意保持保守：

| Platform | Node 20 | Node 22 | Node 24 | Node 26 |
|---|---|---|---|---|
| macOS arm64 (`darwin-arm64`) | Supported | Supported | Supported | Supported |
| macOS x64 (`darwin-x64`) | Supported | Supported | Supported | Supported |
| Linux glibc arm64 (`linux-arm64-gnu`) | Supported | Supported | Supported | Supported |
| Linux glibc x64 (`linux-x64-gnu`) | Supported | Supported | Supported | Supported |
| Windows arm64 MSVC (`win32-arm64-msvc`) | Supported | Supported | Supported | Supported |
| Windows x86 MSVC (`win32-ia32-msvc`) | Supported | Supported | No 32-bit runtime | No 32-bit runtime |
| Windows x64 MSVC (`win32-x64-msvc`) | Supported | Supported | Supported | Supported |

两个产品系列都会为每个受支持的平台发布一个 `napi-v9` 二进制文件，
并在 Node 20、22、24 和 26 上进行测试。Linux GNU 预构建包要求 glibc 2.28 或更新版本，
以及兼容 `GLIBCXX_3.4.25` 的 C++ 运行时。目前尚未发布 Linux musl 预构建包。
Node.js 在 v22 之后停止提供 32 位 Windows 二进制文件，因此 `win32-ia32-msvc`
仅覆盖 Node 20 和 22。

两个产品还会在 Electron 43.0.0、44.0.0 和 45.0.0-alpha.6 的真实主进程中进行测试。
Electron 测试矩阵覆盖 macOS arm64/x64、Linux glibc arm64/x64 和 Windows arm64/x64。
Electron 43 额外覆盖 Windows ia32；后续版本不再发布 ia32 二进制文件。
其他 Electron 版本在完成分析并被明确加入支持范围之前，都会被拒绝使用。
详情请参阅 [docs/support-matrix.md](docs/support-matrix.md) 和
[docs/internal-modules.md](docs/internal-modules.md)。

## 许可证

MIT
