import { createRequire } from 'node:module'
import path from 'node:path'
import { pathToFileURL } from 'node:url'

export type LoadCache = Map<string, Record<string, unknown>>

const requireFromStrategy = createRequire(import.meta.url)

export function fixturesUrlPrefix(fixturesDir: string): string {
  return pathToFileURL(`${path.resolve(fixturesDir)}${path.sep}`).href
}

export function purgeFixtureEsmLoadCache(prefix: string): void {
  const loadCache = getExposeInternalsEsmLoadCache()
  purgeLoadCacheByPrefix(loadCache, prefix)
}

export function purgeLoadCacheByPrefix(loadCache: LoadCache, prefix: string): void {
  for (const url of loadCache.keys()) {
    if (url.startsWith(prefix)) {
      loadCache.delete(url)
    }
  }
}

function getExposeInternalsEsmLoadCache(): LoadCache {
  const loaderModule = requireFromStrategy('internal/modules/esm/loader') as {
    getOrInitializeCascadedLoader(): { loadCache: LoadCache }
  }

  return loaderModule.getOrInitializeCascadedLoader().loadCache
}
