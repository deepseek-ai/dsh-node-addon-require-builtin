import fs from 'node:fs';
import path from 'node:path';

const NAPI_VERSION = '9';
const BINARY_NAME = 'internal_require.node';
const BACKEND_AUTO = 'auto';
const BACKEND_NAPI = 'napi';
const BACKEND_NODEABI = 'nodeabi';

export type NativeBackend = typeof BACKEND_NAPI | typeof BACKEND_NODEABI;
export type BackendPreference = NativeBackend | typeof BACKEND_AUTO;

interface BuildTagOptions {
  napiVersion?: string;
  nodeModuleVersion?: string;
}

interface PrebuiltBinary {
  backend: NativeBackend;
  abi: string;
  path: string;
}

interface PrebuildsManifest {
  platform: string;
  binaries: PrebuiltBinary[];
}

interface NativeBinding {
  getModulesCjsLoader: () => unknown;
  getModulesEsmLoader: () => unknown;
  getNativeBindingInfo: () => NativeBindingInfo;
  bindingPath?: string;
  nativeBindingInfo?: NativeBindingInfo;
  [key: string]: unknown;
}

interface NativeBindingInfo {
  mode: string;
  backend: string;
  abi: string;
}

interface Attempt {
  source?: string;
  request?: string;
  path?: string;
  backend?: string;
  abi?: string;
  message?: string;
  attempts?: unknown;
}

interface AttemptsError extends Error {
  attempts?: Attempt[];
}

export interface LoadedBinding {
  binding: NativeBinding;
  path: string;
  source: string;
  attempts: Attempt[];
}

export interface LoadEntryOptions {
  packageDir: string;
  packagePrefix: string;
  backend?: string;
}

export interface LoadPrebuildOptions {
  backend?: string;
  prebuilds?: PrebuildsManifest;
}

function errorMessage(error: unknown): string | undefined {
  if (error instanceof Error) return error.message;
  if (error === undefined || error === null) return undefined;
  return String(error);
}

function normalizeBackend(value?: string): BackendPreference {
  const backend = value || BACKEND_AUTO;
  if (
    backend === BACKEND_AUTO ||
    backend === BACKEND_NAPI ||
    backend === BACKEND_NODEABI
  ) {
    return backend;
  }
  throw new Error(`unsupported native backend: ${value}`);
}

function normalizeBinaryBackend(value: string): NativeBackend {
  if (value === BACKEND_NAPI || value === BACKEND_NODEABI) {
    return value;
  }
  throw new Error(`unsupported native binary backend: ${value}`);
}

function buildAbiTag(selectedBackend: string, options: BuildTagOptions = {}): string {
  if (normalizeBinaryBackend(selectedBackend) === BACKEND_NAPI) {
    return `napi-v${options.napiVersion || NAPI_VERSION}`;
  }
  return `node-v${options.nodeModuleVersion || process.versions.modules}`;
}

function binaryTag(selectedBackend: string, options: BuildTagOptions = {}): string {
  if (normalizeBinaryBackend(selectedBackend) === BACKEND_NAPI) {
    return buildAbiTag(BACKEND_NAPI, options);
  }
  return `nodeabi-v${options.nodeModuleVersion || process.versions.modules}`;
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

export function platformPackageSuffix(): string {
  return runtimeSuffix();
}

export function optionalPackageName(packagePrefix: string): string {
  return `${packagePrefix}-${platformPackageSuffix()}`;
}

function optionsForBackend(selectedBackend: string): BuildTagOptions {
  return normalizeBinaryBackend(selectedBackend) === BACKEND_NAPI
    ? { napiVersion: NAPI_VERSION }
    : { nodeModuleVersion: process.versions.modules };
}

function prebuiltFileName(
  selectedBackend: string,
  options: BuildTagOptions = optionsForBackend(selectedBackend),
): string {
  return `${platformPackageSuffix()}-${binaryTag(selectedBackend, options)}.node`;
}

export function optionalBinaryRelativePath(
  selectedBackend: string,
  options: BuildTagOptions = optionsForBackend(selectedBackend),
): string {
  return `prebuilt/${prebuiltFileName(selectedBackend, options)}`;
}

function expectedBinary(selectedBackend: string): PrebuiltBinary {
  const backend = normalizeBinaryBackend(selectedBackend);
  const options = optionsForBackend(backend);
  return {
    backend,
    abi: buildAbiTag(backend, options),
    path: optionalBinaryRelativePath(backend, options),
  };
}

function entryCandidateBinaries(backendPreference: BackendPreference): PrebuiltBinary[] {
  const nodeAbiBinary = expectedBinary(BACKEND_NODEABI);
  const napiBinary = expectedBinary(BACKEND_NAPI);

  if (backendPreference === BACKEND_NODEABI) return [nodeAbiBinary];
  if (backendPreference === BACKEND_NAPI) return [napiBinary];
  return [nodeAbiBinary, napiBinary];
}

function currentNodeAbi(): string {
  return `node-v${process.versions.modules}`;
}

function findBinary(
  prebuilds: PrebuildsManifest,
  backend: NativeBackend,
  abi: string,
): PrebuiltBinary | null {
  return prebuilds.binaries.find((binary) =>
    binary.backend === backend && binary.abi === abi
  ) || null;
}

function prebuildCandidateBinaries(
  prebuilds: PrebuildsManifest,
  backendPreference: BackendPreference,
): PrebuiltBinary[] {
  const nodeAbiBinary = findBinary(prebuilds, BACKEND_NODEABI, currentNodeAbi());
  const napiBinary = findBinary(prebuilds, BACKEND_NAPI, 'napi-v9');

  if (backendPreference === BACKEND_NODEABI) {
    return nodeAbiBinary ? [nodeAbiBinary] : [];
  }
  if (backendPreference === BACKEND_NAPI) {
    return napiBinary ? [napiBinary] : [];
  }
  return [nodeAbiBinary, napiBinary].filter((binary): binary is PrebuiltBinary => Boolean(binary));
}

function validateLoadedBinding(binding: NativeBinding, bindingPath: string): NativeBinding {
  if (!binding) {
    throw new Error(`native binding did not export an object: ${bindingPath}`);
  }

  if (typeof binding.getModulesCjsLoader !== 'function') {
    throw new Error(`native binding did not export getModulesCjsLoader(): ${bindingPath}`);
  }
  if (typeof binding.getModulesEsmLoader !== 'function') {
    throw new Error(`native binding did not export getModulesEsmLoader(): ${bindingPath}`);
  }
  if (typeof binding.getNativeBindingInfo !== 'function') {
    throw new Error(`native binding did not export getNativeBindingInfo(): ${bindingPath}`);
  }

  const info = binding.getNativeBindingInfo();
  if (!info || typeof info !== 'object') {
    throw new Error(`native binding info is not an object: ${bindingPath}`);
  }
  if (
    typeof info.mode !== 'string' ||
    typeof info.backend !== 'string' ||
    typeof info.abi !== 'string'
  ) {
    throw new Error(`native binding info has invalid fields: ${bindingPath}`);
  }

  const backend = info.backend;
  if (backend !== BACKEND_NAPI && backend !== BACKEND_NODEABI) {
    throw new Error(`native binding has unsupported backend ${backend}: ${bindingPath}`);
  }

  const expected = buildAbiTag(backend, optionsForBackend(backend));
  if (info.abi !== expected) {
    throw new Error(
      `native binding ABI mismatch for ${bindingPath}: expected ${expected}, got ${info.abi}`,
    );
  }

  Object.defineProperty(binding, 'nativeBindingInfo', {
    value: info,
    enumerable: false,
    configurable: true,
  });
  return binding;
}

function validatePrebuiltBinding(
  binding: NativeBinding,
  binary: PrebuiltBinary,
  bindingPath: string,
): NativeBinding {
  validateLoadedBinding(binding, bindingPath);
  const info = binding.nativeBindingInfo;
  if (!info || info.backend !== binary.backend || info.abi !== binary.abi) {
    const actualBackend = info && info.backend;
    const actualAbi = info && info.abi;
    throw new Error(
      `prebuilt binary mismatch for ${bindingPath}: expected ${binary.backend} ${binary.abi}, got ${actualBackend} ${actualAbi}`,
    );
  }

  Object.defineProperty(binding, 'bindingPath', {
    value: bindingPath,
    enumerable: false,
    configurable: true,
  });
  Object.defineProperty(binding, 'prebuild', {
    value: binary,
    enumerable: false,
    configurable: true,
  });
  return binding;
}

export function loadPrebuild(
  packageDir: string,
  options: LoadPrebuildOptions = {},
): NativeBinding {
  const prebuilds = options.prebuilds || require(path.join(packageDir, 'prebuilds.json')) as PrebuildsManifest;
  const backendPreference = normalizeBackend(
    options.backend || process.env.DSH_NODE_ADDON_INTERNAL_BACKEND,
  );
  const candidates = prebuildCandidateBinaries(prebuilds, backendPreference);
  const attempts: Attempt[] = [];

  if (candidates.length === 0) {
    const error = new Error(
      `No prebuilt binary candidate in ${prebuilds.platform} for ${backendPreference} ${currentNodeAbi()}`,
    ) as AttemptsError;
    error.attempts = attempts;
    throw error;
  }

  for (const binary of candidates) {
    const bindingPath = path.join(packageDir, binary.path);
    try {
      return validatePrebuiltBinding(require(bindingPath) as NativeBinding, binary, bindingPath);
    } catch (error) {
      attempts.push({
        path: bindingPath,
        backend: binary.backend,
        abi: binary.abi,
        message: errorMessage(error),
      });
    }
  }

  const error = new Error(`No usable prebuilt binary found in ${prebuilds.platform}`) as AttemptsError;
  error.attempts = attempts;
  throw error;
}

function tryRequirePackage(candidate: { request: string; source: string }): {
  binding: NativeBinding | null;
  path: string | null;
  error: unknown;
} {
  try {
    const binding = require(candidate.request) as NativeBinding;
    const bindingPath = binding.bindingPath || require.resolve(candidate.request);
    validateLoadedBinding(binding, bindingPath);
    return { binding, path: bindingPath, error: null };
  } catch (error) {
    return { binding: null, path: null, error };
  }
}

function tryRequireLocal(packageDir: string, binary: PrebuiltBinary): {
  binding: NativeBinding | null;
  path: string;
  error: unknown;
} {
  const file = path.join(
    packageDir,
    'build',
    binary.backend,
    `${binary.abi}-${platformPackageSuffix()}`,
    BINARY_NAME,
  );

  try {
    const binding = require(file) as NativeBinding;
    validateLoadedBinding(binding, file);
    Object.defineProperty(binding, 'bindingPath', {
      value: file,
      enumerable: false,
      configurable: true,
    });
    return { binding, path: file, error: null };
  } catch (error) {
    return { binding: null, path: file, error };
  }
}

function pushAttempt(attempts: Attempt[], data: Attempt & { error?: unknown }): void {
  attempts.push({
    ...data,
    message: errorMessage(data.error),
    attempts: data.error && typeof data.error === 'object' && 'attempts' in data.error
      ? (data.error as { attempts?: unknown }).attempts
      : undefined,
  });
}

function noUsableBindingError(
  packagePrefix: string,
  backendPreference: BackendPreference,
  attempts: Attempt[],
): AttemptsError {
  const error = new Error(
    `No usable native binding found for ${optionalPackageName(packagePrefix)} (${backendPreference})`,
  ) as AttemptsError;
  error.attempts = attempts;
  return error;
}

function optionalPackageCandidates(
  packageDir: string,
  packagePrefix: string,
): Array<{ request: string; source: string }> {
  const candidates = [{
    request: optionalPackageName(packagePrefix),
    source: 'optional-package',
  }];

  const workspacePath = path.join(packageDir, '..', platformPackageSuffix());
  if (fs.existsSync(workspacePath)) {
    candidates.push({
      request: workspacePath,
      source: 'optional-package-workspace',
    });
  }

  return candidates;
}

export function loadEntry(options: LoadEntryOptions): LoadedBinding {
  const packageDir = options.packageDir;
  const packagePrefix = options.packagePrefix;
  const backendPreference = normalizeBackend(
    options.backend || process.env.DSH_NODE_ADDON_INTERNAL_BACKEND,
  );
  const attempts: Attempt[] = [];

  if (process.env.DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE !== '1') {
    for (const candidate of optionalPackageCandidates(packageDir, packagePrefix)) {
      const result = tryRequirePackage(candidate);
      if (result.binding && result.path) {
        return {
          binding: result.binding,
          path: result.path,
          source: candidate.source,
          attempts,
        };
      }
      pushAttempt(attempts, {
        source: candidate.source,
        request: candidate.request,
        error: result.error,
      });
    }
  }

  if (process.env.DSH_NODE_ADDON_INTERNAL_DISABLE_LOCAL_BUILD === '1') {
    throw noUsableBindingError(packagePrefix, backendPreference, attempts);
  }

  for (const binary of entryCandidateBinaries(backendPreference)) {
    const result = tryRequireLocal(packageDir, binary);
    if (result.binding) {
      return {
        binding: result.binding,
        path: result.path,
        source: 'local-build',
        attempts,
      };
    }
    pushAttempt(attempts, {
      source: 'local-build',
      path: result.path,
      backend: binary.backend,
      abi: binary.abi,
      error: result.error,
    });
  }

  throw noUsableBindingError(packagePrefix, backendPreference, attempts);
}

export function localBindingPath(packageDir: string, selectedBackend: string): string {
  const backend = normalizeBinaryBackend(selectedBackend);
  const abi = buildAbiTag(backend, optionsForBackend(backend));
  return path.join(
    packageDir,
    'build',
    backend,
    `${abi}-${platformPackageSuffix()}`,
    BINARY_NAME,
  );
}
