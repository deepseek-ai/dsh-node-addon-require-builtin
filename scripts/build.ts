import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import {
  joinPathList,
  relativePathForLog,
  splitPathList,
} from './path-utils.js';

const BACKEND_NAPI = 'napi';
const BACKEND_NODEABI = 'nodeabi';
const BINARY_NAME = 'internal_require.node';

type NativeBackend = typeof BACKEND_NAPI | typeof BACKEND_NODEABI;

interface BuildTagOptions {
  napiVersion?: string;
  nodeModuleVersion?: string;
}

const root = path.resolve(__dirname, '..');
const packageRoot = path.join(root, 'packages', 'entry');
const backend = normalizeBackend(
  process.env.DSH_NODE_ADDON_INTERNAL_BACKEND,
);
const napiVersion = process.env.NAPI_VERSION || '9';
const buildOptions: BuildTagOptions = backend === BACKEND_NAPI
  ? { napiVersion }
  : { nodeModuleVersion: process.versions.modules };
const outputMode = process.env.DSH_NODE_ADDON_INTERNAL_BUILD_OUTPUT || 'build';
const commonSources = [
  path.join(packageRoot, 'src', 'node_api_addon.cc'),
  path.join(packageRoot, 'src', 'debug_trace.cc'),
  path.join(packageRoot, 'src', 'native_types.cc'),
  path.join(packageRoot, 'src', 'internal_require_probe.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'helper.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'platform.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'darwin_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'darwin_x64.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'linux_glibc_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'linux_glibc_x64.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'win32_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_context', 'win32_x64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'helper.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'platform.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'getter_decoder.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'posix.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'win32_common.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'darwin_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'darwin_x64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'linux_glibc_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'linux_glibc_x64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'win32_arm64.cc'),
  path.join(packageRoot, 'src', 'runtime_probe', 'win32_x64.cc'),
];
const sources = [
  ...commonSources,
  path.join(
    root,
    'packages',
    'entry',
    'src',
    backend === 'napi' ? 'runtime_compat_napi.cc' : 'runtime_compat_nodeabi.cc',
  ),
];
function buildOutputPath(): string {
  if (outputMode === 'build') {
    return path.join(packageRoot, 'build', localBuildSubdir(backend, buildOptions), BINARY_NAME);
  }

  if (outputMode === 'prebuild') {
    const suffix = runtimeSuffix();
    const packageDir = path.join(root, 'packages', suffix);
    if (!fs.existsSync(path.join(packageDir, 'package.json'))) {
      throw new Error(`unsupported prebuild package platform: ${suffix}`);
    }
    return path.join(
      packageDir,
      prebuiltRelativePath(suffix, backend, buildOptions),
    );
  }

  throw new Error(`unsupported DSH_NODE_ADDON_INTERNAL_BUILD_OUTPUT: ${outputMode}`);
}

const output = buildOutputPath();
const outDir = path.dirname(output);

function normalizeBackend(value?: string): NativeBackend {
  const backend = value || BACKEND_NAPI;
  if (backend === BACKEND_NAPI || backend === BACKEND_NODEABI) {
    return backend;
  }
  throw new Error(`unsupported native backend: ${value}`);
}

function backendMacro(selectedBackend: NativeBackend): '1' | '2' {
  return selectedBackend === BACKEND_NAPI ? '1' : '2';
}

function buildAbiTag(
  selectedBackend: NativeBackend,
  options: BuildTagOptions = {},
): string {
  if (selectedBackend === BACKEND_NAPI) {
    return `napi-v${options.napiVersion || '9'}`;
  }
  return `node-v${options.nodeModuleVersion || process.versions.modules}`;
}

function binaryTag(
  selectedBackend: NativeBackend,
  options: BuildTagOptions = {},
): string {
  if (selectedBackend === BACKEND_NAPI) {
    return buildAbiTag(BACKEND_NAPI, options);
  }
  return `nodeabi-v${options.nodeModuleVersion || process.versions.modules}`;
}

function localBuildSubdir(
  selectedBackend: NativeBackend,
  options: BuildTagOptions = {},
): string {
  return `${selectedBackend}/${buildAbiTag(selectedBackend, options)}-${runtimeSuffix()}`;
}

function prebuiltRelativePath(
  suffix: string,
  selectedBackend: NativeBackend,
  options: BuildTagOptions = {},
): string {
  return `prebuilt/${suffix}-${binaryTag(selectedBackend, options)}.node`;
}

function linuxLibc(): 'glibc' | 'musl' | undefined {
  if (process.platform !== 'linux') return undefined;
  const report = process.report && typeof process.report.getReport === 'function'
    ? process.report.getReport() as { header?: { glibcVersionRuntime?: string } }
    : null;
  return report && report.header && report.header.glibcVersionRuntime
    ? 'glibc'
    : 'musl';
}

function runtimeSuffix(): string {
  if (process.platform === 'darwin') return `darwin-${process.arch}`;
  if (process.platform === 'linux') {
    const libc = linuxLibc() === 'glibc' ? 'gnu' : 'musl';
    return `linux-${process.arch}-${libc}`;
  }
  if (process.platform === 'win32') return `win32-${process.arch}-msvc`;
  return `${process.platform}-${process.arch}`;
}

function nodePrefix(): string {
  return path.resolve(path.dirname(process.execPath), '..');
}

function nodeIncludeDir(): string {
  if (process.env.NODE_INCLUDE_DIR) return process.env.NODE_INCLUDE_DIR;
  return path.join(nodePrefix(), 'include', 'node');
}

function nodeHeaderIncludeDirs(): string[] {
  const explicit = process.env.NODE_JS_PUBLIC_INCLUDE_DIRS;
  if (explicit) return splitPathList(explicit);

  throw new Error('nodeabi backend requires NODE_JS_PUBLIC_INCLUDE_DIRS');
}

function nativeIncludeArgs(includeDir: string): string[] {
  const dirs = backend === 'nodeabi'
    ? [...nodeHeaderIncludeDirs(), includeDir]
    : [includeDir];
  return dirs.flatMap((dir) => ['-I', dir]);
}

function commonArgs(includeDir: string): string[] {
  const cxxStandard = backend === 'napi' ? '-std=c++17' : '-std=c++20';
  const args = [
    cxxStandard,
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-cast-function-type-mismatch',
    '-fno-exceptions',
    '-fvisibility=hidden',
    '-DNAPI_VERSION=' + napiVersion,
    '-DINTERNAL_REQUIRE_BACKEND=' + backendMacro(backend),
    '-DNODE_ADDON_API_DISABLE_CPP_EXCEPTIONS',
    '-DNODE_GYP_MODULE_NAME=internal_require',
    ...nativeIncludeArgs(includeDir),
    '-I',
    nodeAddonApiIncludeDir(),
    ...sources,
    '-o',
    output,
  ];
  if (backend === 'nodeabi') {
    args.splice(8, 0, '-DHAVE_SQLITE=0', '-DHAVE_AMARO=0');
  }
  return args;
}

function compileArgs(includeDir: string): string[] {
  const args = commonArgs(includeDir);
  if (process.platform === 'darwin') {
    return ['-bundle', '-undefined', 'dynamic_lookup', ...args];
  }
  if (process.platform === 'linux') {
    return ['-shared', '-fPIC', '-pthread', ...args];
  }
  throw new Error(`unsupported platform for direct build: ${process.platform}`);
}

function assertNodeHeaders(includeDir: string): void {
  const dirs = backend === 'nodeabi' ? nodeHeaderIncludeDirs() : [includeDir];
  const found = dirs.some((dir) => fs.existsSync(path.join(dir, 'node.h')));
  if (!found) {
    throw new Error(`Node.js public headers not found under ${joinPathList(dirs)}`);
  }
}

// Windows has no drop-in clang `-bundle`/`-shared` invocation, so the addon is
// built with node-gyp + MSVC there (the same toolchain the published install
// script uses). binding.gyp selects the backend through GYP_DEFINES.
function buildWithNodeGyp(): void {
  if (backend === BACKEND_NODEABI) {
    assertNodeHeaders(nodeIncludeDir());
  }

  const nodeGyp = require.resolve('node-gyp/bin/node-gyp.js', {
    paths: [packageRoot, root],
  });
  const gypArgs = [
    nodeGyp,
    'rebuild',
    '--enable-lto=false',
    '--enable-thin-lto=false',
    '--lto-jobs=',
  ];
  console.log(`Building ${relativePathForLog(root, output)} (${backend}) with node-gyp`);
  const result = spawnSync(process.execPath, gypArgs, {
    cwd: packageRoot,
    stdio: 'inherit',
    env: {
      ...process.env,
      DSH_NODE_ADDON_INTERNAL_BACKEND: backend,
      GYP_DEFINES: [
        process.env.GYP_DEFINES,
        `internal_require_backend=${backend}`,
        'enable_lto=false',
        'enable_thin_lto=false',
        'lto_jobs=',
      ].filter(Boolean).join(' '),
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }

  const gypOutput = path.join(packageRoot, 'build', 'Release', BINARY_NAME);
  if (!fs.existsSync(gypOutput)) {
    throw new Error(`node-gyp output not found: ${gypOutput}`);
  }
  fs.mkdirSync(outDir, { recursive: true });
  fs.copyFileSync(gypOutput, output);
  console.log(`Copied ${relativePathForLog(root, gypOutput)} -> ${relativePathForLog(root, output)}`);
}

function main(): void {
  if (process.platform === 'win32') {
    buildWithNodeGyp();
    return;
  }

  const includeDir = nodeIncludeDir();
  assertNodeHeaders(includeDir);

  fs.mkdirSync(outDir, { recursive: true });

  const compiler = process.env.CXX || 'c++';
  const args = compileArgs(includeDir);
  console.log(`Building ${relativePathForLog(root, output)} (${backend}) with ${process.execPath}`);
  const result = spawnSync(compiler, args, {
    cwd: root,
    stdio: 'inherit',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function nodeAddonApiIncludeDir(): string {
  const candidates = [
    path.join(packageRoot, 'node_modules', 'node-addon-api'),
    path.join(root, 'node_modules', 'node-addon-api'),
  ];
  const found = candidates.find((candidate) => fs.existsSync(candidate));
  if (!found) {
    throw new Error(`node-addon-api include directory not found under ${joinPathList(candidates)}`);
  }
  return found;
}

main();
