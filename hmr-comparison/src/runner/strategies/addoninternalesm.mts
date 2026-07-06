import type { Scenario, Strategy } from '../types.mjs'
import { fixturesUrlPrefix } from './internalesm-utils.mjs'
import { purgeFixtureEsmLoadCacheViaAddon } from './addoninternalesm-utils.mjs'
import { loadNative } from './utils.mjs'

let esmUrlPrefix = ''
let loadCount = 0

export const addonInternalEsmStrategy: Strategy = {
  name: 'addoninternalesm',
  nodeArgs: ['--expose-gc'],

  async setup(fixturesDir: string) {
    loadCount = 0
    esmUrlPrefix = fixturesUrlPrefix(fixturesDir)
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (scenario.format === 'esm' && loadCount > 0) {
      purgeFixtureEsmLoadCacheViaAddon(esmUrlPrefix)
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
