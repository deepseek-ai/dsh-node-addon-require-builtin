import type { Scenario, Strategy } from '../types.mjs'
import { loadNative } from './utils.mjs'

export const naiveStrategy: Strategy = {
  name: 'naive',
  nodeArgs: ['--expose-gc'],

  async setup() {},

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    return loadNative(entry, scenario)
  },

  async teardown() {},
}
