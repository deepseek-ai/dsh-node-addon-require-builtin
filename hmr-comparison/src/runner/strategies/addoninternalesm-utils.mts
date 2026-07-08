import { createRequire } from 'node:module'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

import { purgeLoadCacheByPrefix, type LoadCache } from './internalesm-utils.mjs'

type RequireBuiltinAddon = {
  requireBuiltin(moduleId: string): unknown
}

type EsmLoaderModule = {
  getOrInitializeCascadedLoader(): { loadCache: LoadCache }
}

const requireFromStrategy = createRequire(import.meta.url)
const comparisonRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '../../..',
)
const addonRoot = path.resolve(comparisonRoot, '..', 'packages', 'entry')

export function purgeFixtureEsmLoadCacheViaAddon(prefix: string): void {
  const loadCache = getAddonEsmLoadCache()
  purgeLoadCacheByPrefix(loadCache, prefix)
}

function getAddonEsmLoadCache(): LoadCache {
  const addon = requireFromStrategy(addonRoot) as RequireBuiltinAddon
  const loaderModule = addon.requireBuiltin('internal/modules/esm/loader') as EsmLoaderModule

  return loaderModule.getOrInitializeCascadedLoader().loadCache
}
