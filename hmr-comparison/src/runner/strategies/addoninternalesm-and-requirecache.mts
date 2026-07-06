import type { Scenario, Strategy } from '../types.mjs'
import { fixturesUrlPrefix } from './internalesm-utils.mjs'
import { purgeFixtureEsmLoadCacheViaAddon } from './addoninternalesm-utils.mjs'
import { loadNative, purgeRequireCache } from './utils.mjs'

let esmUrlPrefix = ''
let loadCount = 0

export const addonInternalEsmAndRequirecacheStrategy: Strategy = {
  name: 'addoninternalesm-and-requirecache',
  nodeArgs: ['--expose-gc'],

  async setup(fixturesDir: string) {
    loadCount = 0
    esmUrlPrefix = fixturesUrlPrefix(fixturesDir)
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (loadCount > 0) {
      if (scenario.format === 'esm') {
        purgeFixtureEsmLoadCacheViaAddon(esmUrlPrefix)
      } else if (scenario.format === 'cjs') {
        purgeRequireCache(entry)
      }
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
