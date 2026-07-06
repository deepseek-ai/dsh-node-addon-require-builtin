import vm from 'node:vm'
import { pathToFileURL } from 'node:url'

import type { Scenario, Strategy } from '../types.mjs'

type ImportResult = {
  value?: unknown
  payload?: object
  default?: unknown
}

export const vmcontextStrategy: Strategy = {
  name: 'vmcontext',
  nodeArgs: ['--expose-gc'],

  async setup() {},

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    const entryUrl = pathToFileURL(entry).href
    const context = vm.createContext({ entryUrl })
    const script = new vm.Script('import(entryUrl)', {
      importModuleDynamically: vm.constants.USE_MAIN_CONTEXT_DEFAULT_LOADER,
    })
    const namespace = await script.runInContext(context) as ImportResult

    if (scenario.format === 'cjs' && namespace.default && typeof namespace.default === 'object') {
      return namespace.default
    }

    return namespace
  },

  async teardown() {},
}
