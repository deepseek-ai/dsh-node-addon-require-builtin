import { createRequire } from 'node:module'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

import { purgeLoadCacheByPrefix, type LoadCache } from './internalesm-utils.mjs'

type InternalRequireAddon = {
  getModulesEsmLoader(): unknown
}

const requireFromStrategy = createRequire(import.meta.url)
const comparisonRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '../../..',
)
const addonRoot = path.resolve(comparisonRoot, '..')

export function purgeFixtureEsmLoadCacheViaAddon(prefix: string): void {
  const loadCache = getAddonEsmLoadCache()
  purgeLoadCacheByPrefix(loadCache, prefix)
}

function getAddonEsmLoadCache(): LoadCache {
  process.env.DSH_NODE_ADDON_INTERNAL_DISABLE_OPTIONAL_PACKAGE ??= '1'

  const addon = requireFromStrategy(addonRoot) as InternalRequireAddon
  const loaderModule = addon.getModulesEsmLoader() as {
    getOrInitializeCascadedLoader(): { loadCache: LoadCache }
  }

  return loaderModule.getOrInitializeCascadedLoader().loadCache
}
