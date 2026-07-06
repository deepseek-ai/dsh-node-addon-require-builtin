import type { Scenario, Strategy } from '../types.mjs'
import { fixturesUrlPrefix, purgeFixtureEsmLoadCache } from './internalesm-utils.mjs'
import { loadNative, purgeRequireCache } from './utils.mjs'

let esmUrlPrefix = ''
let loadCount = 0

export const internalesmAndRequirecacheStrategy: Strategy = {
  name: 'internalesm-and-requirecache',
  nodeArgs: ['--expose-gc', '--expose-internals'],

  async setup(fixturesDir: string) {
    loadCount = 0
    esmUrlPrefix = fixturesUrlPrefix(fixturesDir)
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (loadCount > 0) {
      if (scenario.format === 'esm') {
        purgeFixtureEsmLoadCache(esmUrlPrefix)
      } else if (scenario.format === 'cjs') {
        purgeRequireCache(entry)
      }
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
