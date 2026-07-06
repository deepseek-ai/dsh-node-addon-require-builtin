import { existsSync } from 'node:fs'
import { readFile } from 'node:fs/promises'
import { createRequire } from 'node:module'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

import type { Scenario, Strategy } from '../types.mjs'

type IsolatedVm = any
type Isolate = any
type Context = any
type Script = any
type Module = any
type Reference = {
  getSync(property: string, options: { copy: true }): unknown
  getSync(property: string, options: { reference: true }): Reference
  release(): void
}

type ModuleRecord = {
  format: 'cjs'
  source: string
  dirname: string
}

type Resource = {
  isolate: Isolate
  context: Context
  scripts: Script[]
  modules: Module[]
  references: Reference[]
}

const requireFromStrategy = createRequire(import.meta.url)

let fixturesDir = ''
let currentResource: Resource | undefined

export function isIsolatedVmAvailable(): boolean {
  try {
    requireFromStrategy.resolve('isolated-vm')
    return true
  } catch {
    return false
  }
}

export const isolatedvmStrategy: Strategy = {
  name: 'isolatedvm',
  nodeArgs: ['--expose-gc', '--no-node-snapshot'],

  async setup(nextFixturesDir: string) {
    fixturesDir = nextFixturesDir
    disposeCurrentResource()
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    disposeCurrentResource()

    if (scenario.format === 'esm') {
      return loadEsm(entry)
    }

    if (scenario.format === 'cjs') {
      return loadCjs(entry)
    }

    throw new Error(`Unsupported scenario format: ${scenario.format}`)
  },

  async teardown() {
    disposeCurrentResource()
  },
}

async function loadEsm(entry: string): Promise<{ value: unknown; payload: object }> {
  const ivm = getIsolatedVm()
  const isolate = new ivm.Isolate({ memoryLimit: 8 })
  const context = isolate.createContextSync()
  const modules: Module[] = []
  const references: Reference[] = []
  const scripts: Script[] = []
  const modulesByUrl = new Map<string, Module>()
  const urlsByModule = new WeakMap<Module, string>()

  const getModule = async (url: string): Promise<Module> => {
    const existing = modulesByUrl.get(url)
    if (existing) return existing

    const filename = fileURLToPath(url)
    assertInsideFixtures(filename)

    const source = await readFile(filename, 'utf8')
    const module = isolate.compileModuleSync(source, { filename })
    modulesByUrl.set(url, module)
    urlsByModule.set(module, url)
    modules.push(module)
    return module
  }

  const entryModule = await getModule(pathToFileURL(entry).href)
  await entryModule.instantiate(context, async (specifier: string, referrer: Module) => {
    const parentUrl = urlsByModule.get(referrer)
    if (!parentUrl) throw new Error('Unknown isolated-vm module referrer')
    return getModule(resolveFixtureSpecifier(specifier, parentUrl))
  })
  entryModule.evaluateSync()

  const namespace = entryModule.namespace as Reference
  const value = namespace.getSync('value', { copy: true })
  const payload = namespace.getSync('payload', { reference: true }) as Reference
  references.push(namespace, payload)
  currentResource = { isolate, context, scripts, modules, references }

  return { value, payload }
}

async function loadCjs(entry: string): Promise<{ value: unknown; payload: object }> {
  const ivm = getIsolatedVm()
  const isolate = new ivm.Isolate({ memoryLimit: 8 })
  const context = isolate.createContextSync()
  const modules = await collectCjsModules(entry)
  const runtimeSource = buildCjsRuntime(entry, modules)
  const script = isolate.compileScriptSync(runtimeSource, { filename: 'isolatedvm-cjs-runtime.js' })
  const result = script.runSync(context, { reference: true }) as Reference
  const value = result.getSync('value', { copy: true })
  const payload = result.getSync('payload', { reference: true }) as Reference

  currentResource = {
    isolate,
    context,
    scripts: [script],
    modules: [],
    references: [result, payload],
  }

  return { value, payload }
}

async function collectCjsModules(entry: string): Promise<Record<string, ModuleRecord>> {
  const records: Record<string, ModuleRecord> = {}

  const visit = async (filename: string): Promise<void> => {
    assertInsideFixtures(filename)
    if (records[filename]) return

    const source = await readFile(filename, 'utf8')
    records[filename] = {
      format: 'cjs',
      source,
      dirname: path.dirname(filename),
    }

    for (const specifier of findRequireSpecifiers(source)) {
      await visit(resolveFixturePath(specifier, filename))
    }
  }

  await visit(entry)
  return records
}

function buildCjsRuntime(entry: string, records: Record<string, ModuleRecord>): string {
  return `
const __entry = ${JSON.stringify(entry)};
const __records = ${JSON.stringify(records)};
const __cache = Object.create(null);

function __resolve(specifier, parentId) {
  if (!specifier.startsWith('.')) {
    throw new Error('isolatedvm CJS runtime only supports relative fixture requires: ' + specifier);
  }

  const parentDir = __records[parentId].dirname;
  const normalized = parentDir + '/' + specifier.replace(/^\\.\\//, '');
  const candidates = [normalized, normalized + '.cjs', normalized + '.js'];
  for (const candidate of candidates) {
    if (__records[candidate]) return candidate;
  }

  throw new Error('Cannot resolve fixture require ' + specifier + ' from ' + parentId);
}

function __load(id) {
  if (__cache[id]) return __cache[id].exports;
  const record = __records[id];
  if (!record) throw new Error('Missing isolatedvm CJS module: ' + id);

  const module = { exports: {} };
  __cache[id] = module;
  const require = (specifier) => __load(__resolve(specifier, id));
  const wrapper = new Function('require', 'module', 'exports', record.source + '\\n//# sourceURL=' + id);
  wrapper(require, module, module.exports);
  return module.exports;
}

const __exports = __load(__entry);
({ value: __exports.value, payload: __exports.payload });
`
}

function findRequireSpecifiers(source: string): string[] {
  return [...source.matchAll(/require\(\s*['"]([^'"]+)['"]\s*\)/g)]
    .map((match) => match[1])
}

function resolveFixtureSpecifier(specifier: string, parentUrl: string): string {
  const resolved = new URL(specifier, parentUrl)
  assertInsideFixtures(fileURLToPath(resolved))
  return resolved.href
}

function resolveFixturePath(specifier: string, parentFilename: string): string {
  if (!specifier.startsWith('.')) {
    throw new Error(`isolatedvm only supports relative fixture requires: ${specifier}`)
  }

  const base = path.resolve(path.dirname(parentFilename), specifier)
  const candidates = [base, `${base}.cjs`, `${base}.js`]
  const found = candidates.find((candidate) => {
    const relative = path.relative(fixturesDir, candidate)
    return !relative.startsWith('..') && !path.isAbsolute(relative) && existsSync(candidate)
  })
  if (!found) throw new Error(`Cannot resolve fixture require ${specifier} from ${parentFilename}`)
  return found
}

function assertInsideFixtures(filename: string): void {
  const relative = path.relative(fixturesDir, filename)
  if (relative.startsWith('..') || path.isAbsolute(relative)) {
    throw new Error(`Refusing to load module outside fixtures: ${filename}`)
  }
}

function getIsolatedVm(): IsolatedVm {
  return requireFromStrategy('isolated-vm')
}

function disposeCurrentResource(): void {
  if (!currentResource) return

  for (const reference of currentResource.references) {
    safeRelease(reference)
  }
  for (const module of currentResource.modules) {
    safeRelease(module)
  }
  for (const script of currentResource.scripts) {
    safeRelease(script)
  }

  safeRelease(currentResource.context)
  if (!currentResource.isolate.isDisposed) {
    currentResource.isolate.dispose()
  }
  currentResource = undefined
}

function safeRelease(releasable: { release(): void }): void {
  try {
    releasable.release()
  } catch {
    // Releasing already-disposed isolated-vm handles is harmless for this probe.
  }
}
