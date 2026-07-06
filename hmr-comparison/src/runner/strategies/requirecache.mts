import type { Scenario, Strategy } from '../types.mjs'
import { loadNative, purgeRequireCache } from './utils.mjs'

let loadCount = 0

export const requirecacheStrategy: Strategy = {
  name: 'requirecache',
  nodeArgs: ['--expose-gc'],

  async setup() {
    loadCount = 0
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (scenario.format === 'cjs' && loadCount > 0) {
      purgeRequireCache(entry)
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
