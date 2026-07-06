import type { Scenario, Strategy } from '../types.mjs'
import { fixturesUrlPrefix, purgeFixtureEsmLoadCache } from './internalesm-utils.mjs'
import { loadNative } from './utils.mjs'

let esmUrlPrefix = ''
let loadCount = 0

export const internalesmStrategy: Strategy = {
  name: 'internalesm',
  nodeArgs: ['--expose-gc', '--expose-internals'],

  async setup(fixturesDir: string) {
    loadCount = 0
    esmUrlPrefix = fixturesUrlPrefix(fixturesDir)
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (scenario.format === 'esm' && loadCount > 0) {
      purgeFixtureEsmLoadCache(esmUrlPrefix)
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
