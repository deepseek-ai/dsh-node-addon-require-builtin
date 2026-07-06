import { readFile } from 'node:fs/promises'
import path from 'node:path'
import vm from 'node:vm'
import { fileURLToPath, pathToFileURL } from 'node:url'

import type { Scenario, Strategy } from '../types.mjs'
import { loadNative } from './utils.mjs'

type SourceTextModule = vm.Module & {
  identifier: string
  namespace: object
  link(linker: (specifier: string, referencingModule: SourceTextModule) => Promise<SourceTextModule>): Promise<void>
  evaluate(): Promise<void>
}

type SourceTextModuleConstructor = new (
  sourceText: string,
  options: { context: vm.Context; identifier: string },
) => SourceTextModule

interface VmSourceTextModuleStrategyOptions {
  name: string
  nodeArgs?: string[]
  setup?(fixturesDir: string): void | Promise<void>
  teardown?(): void | Promise<void>
}

let fixturesDir = ''

export const vmsourcetextmoduleStrategy = createVmSourceTextModuleStrategy({
  name: 'vmsourcetextmodule',
})

export function createVmSourceTextModuleStrategy({
  name,
  nodeArgs = ['--expose-gc', '--experimental-vm-modules'],
  setup,
  teardown,
}: VmSourceTextModuleStrategyOptions): Strategy {
  return {
    name,
    nodeArgs,

    async setup(nextFixturesDir: string) {
      fixturesDir = nextFixturesDir
      await setup?.(nextFixturesDir)
    },

    async load(entry: string, scenario: Scenario): Promise<unknown> {
      if (scenario.format !== 'esm') {
        return loadNative(entry, scenario)
      }

      return loadEsmGraph(entry)
    },

    async teardown() {
      await teardown?.()
    },
  }
}

async function loadEsmGraph(entry: string): Promise<object> {
  const context = vm.createContext({ ArrayBuffer, Math })
  const modules = new Map<string, SourceTextModule>()
  const entryModule = await getModule(pathToFileURL(entry).href, context, modules)

  await entryModule.link(async (specifier, referencingModule) => {
    const childUrl = resolveFixtureSpecifier(specifier, referencingModule.identifier)
    return getModule(childUrl, context, modules)
  })
  await entryModule.evaluate()

  return entryModule.namespace
}

async function getModule(
  url: string,
  context: vm.Context,
  modules: Map<string, SourceTextModule>,
): Promise<SourceTextModule> {
  const existing = modules.get(url)
  if (existing) return existing

  const filename = fileURLToPath(url)
  assertInsideFixtures(filename)

  const source = await readFile(filename, 'utf8')
  const ModuleConstructor = vm.SourceTextModule as SourceTextModuleConstructor | undefined
  if (!ModuleConstructor) {
    throw new Error('vm.SourceTextModule is unavailable; run with --experimental-vm-modules.')
  }

  const module = new ModuleConstructor(source, { context, identifier: url })
  modules.set(url, module)
  return module
}

function resolveFixtureSpecifier(specifier: string, parentUrl: string): string {
  const resolved = new URL(specifier, parentUrl)
  assertInsideFixtures(fileURLToPath(resolved))
  return resolved.href
}

function assertInsideFixtures(filename: string): void {
  const relative = path.relative(fixturesDir, filename)
  if (relative.startsWith('..') || path.isAbsolute(relative)) {
    throw new Error(`Refusing to load module outside fixtures: ${filename}`)
  }
}
