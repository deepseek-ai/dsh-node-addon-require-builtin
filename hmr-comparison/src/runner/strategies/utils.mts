import { createRequire } from 'node:module'
import { pathToFileURL } from 'node:url'

import type { Scenario } from '../types.mjs'

type CacheModule = {
  id: string
  children: CacheModule[]
}

export const requireFromRunner = createRequire(import.meta.url)

export async function loadNative(entry: string, scenario: Scenario): Promise<unknown> {
  if (scenario.format === 'esm') {
    return import(pathToFileURL(entry).href)
  }

  if (scenario.format === 'cjs') {
    return createRequire(pathToFileURL(entry))(entry)
  }

  throw new Error(`Unsupported scenario format: ${scenario.format}`)
}

export function purgeRequireCache(entry: string): void {
  const entryRequire = createRequire(pathToFileURL(entry))
  const resolved = entryRequire.resolve(entry)
  purgeCacheModule(resolved, new Set<string>())
}

function purgeCacheModule(moduleId: string, seen: Set<string>): void {
  if (seen.has(moduleId)) return
  seen.add(moduleId)

  const cachedModule = requireFromRunner.cache[moduleId] as CacheModule | undefined
  if (!cachedModule) return

  for (const child of cachedModule.children) {
    purgeCacheModule(child.id, seen)
  }

  for (const module of Object.values(requireFromRunner.cache) as CacheModule[]) {
    module.children = module.children.filter((child) => child.id !== moduleId)
  }

  cachedModule.children = []
  delete requireFromRunner.cache[moduleId]
}
