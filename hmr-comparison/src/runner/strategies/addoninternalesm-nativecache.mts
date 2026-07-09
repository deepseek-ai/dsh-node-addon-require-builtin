import type { Scenario, Strategy } from '../types.mjs'
import { fixturesUrlPrefix } from './internalesm-utils.mjs'
import {
  installAddonNativeCacheLoader,
  purgeFixtureEsmLoadCacheViaAddon,
  uninstallAddonNativeCacheLoader,
} from './addoninternalesm-utils.mjs'
import { loadNative } from './utils.mjs'

let esmUrlPrefix = ''
let loadCount = 0

export const addonInternalEsmNativecacheStrategy: Strategy = {
  name: 'addoninternalesm-nativecache',
  nodeArgs: ['--expose-gc'],

  async setup(fixturesDir: string) {
    loadCount = 0
    esmUrlPrefix = fixturesUrlPrefix(fixturesDir)
    installAddonNativeCacheLoader()
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (scenario.format === 'esm' && loadCount > 0) {
      purgeFixtureEsmLoadCacheViaAddon(esmUrlPrefix)
    }

    loadCount += 1
    return loadNative(entry, scenario)
  },

  async teardown() {
    uninstallAddonNativeCacheLoader()
  },
}
