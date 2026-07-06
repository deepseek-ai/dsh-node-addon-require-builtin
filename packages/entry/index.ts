import path from 'node:path';

interface LoadedBinding {
  binding: {
    getModulesCjsLoader: () => unknown;
    getModulesEsmLoader: () => unknown;
    getNativeBindingInfo: () => NativeBindingInfo;
  };
  path: string;
  source: string;
}

interface NativeBindingInfo {
  mode: string;
  backend: string;
  abi: string;
}

export interface BindingInfo extends NativeBindingInfo {
  bindingPath: string;
  bindingSource: string;
  localBindingPath: string;
  optionalPackageName: string;
  optionalBinaryRelativePath: string;
  platformPackageSuffix: string;
}

interface LoaderApi {
  loadEntry(options: { packageDir: string; packagePrefix: string }): LoadedBinding;
  localBindingPath(packageDir: string, selectedBackend: string): string;
  optionalBinaryRelativePath(selectedBackend: string): string;
  optionalPackageName(packagePrefix: string): string;
  platformPackageSuffix(): string;
}

const {
  loadEntry,
  localBindingPath: resolveLocalBindingPath,
  optionalBinaryRelativePath,
  optionalPackageName: resolveOptionalPackageName,
  platformPackageSuffix,
} = require('@deepseek-ai/dsh-node-addon-internal-loader') as LoaderApi;

const PACKAGE_PREFIX = '@deepseek-ai/dsh-node-addon-internal';
const packageDir = path.resolve(__dirname, '..');
const loadedBinding = loadEntry({
  packageDir,
  packagePrefix: PACKAGE_PREFIX,
});
const binding = loadedBinding.binding;
let bindingInfo: Readonly<BindingInfo> | undefined;

export function getModulesCjsLoader(): unknown {
  return binding.getModulesCjsLoader();
}

export function getModulesEsmLoader(): unknown {
  return binding.getModulesEsmLoader();
}

export function getBindingInfo(): Readonly<BindingInfo> {
  if (bindingInfo) return bindingInfo;
  const nativeInfo = binding.getNativeBindingInfo();
  bindingInfo = Object.freeze({
    mode: nativeInfo.mode,
    backend: nativeInfo.backend,
    abi: nativeInfo.abi,
    bindingPath: loadedBinding.path,
    bindingSource: loadedBinding.source,
    localBindingPath: resolveLocalBindingPath(packageDir, nativeInfo.backend),
    optionalPackageName: resolveOptionalPackageName(PACKAGE_PREFIX),
    optionalBinaryRelativePath: optionalBinaryRelativePath(nativeInfo.backend),
    platformPackageSuffix: platformPackageSuffix(),
  });
  return bindingInfo;
}

const api = {
  getModulesCjsLoader,
  getModulesEsmLoader,
  getBindingInfo,
};

export default api;
