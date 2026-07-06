import { registerHooks } from 'node:module'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

import type { Scenario, Strategy } from '../types.mjs'
import { loadNative, purgeRequireCache } from './utils.mjs'

let fixturesUrlPrefix = ''
let hookHandle: ReturnType<typeof registerHooks> | undefined
let version = 0

export const hookversionAndRequirecacheStrategy: Strategy = {
  name: 'hookversion-and-requirecache',
  nodeArgs: ['--expose-gc'],

  async setup(fixturesDir: string) {
    version = 0
    fixturesUrlPrefix = pathToFileURL(`${path.resolve(fixturesDir)}${path.sep}`).href

    hookHandle = registerHooks({
      resolve(specifier, context, nextResolve) {
        const result = nextResolve(specifier, context)
        if (version === 0 || !isFixtureFileUrl(result.url)) return result

        const cleanUrl = stripSearchAndHash(result.url)
        return {
          ...result,
          url: `${cleanUrl}?v=${version}`,
        }
      },
    })
  },

  async load(entry: string, scenario: Scenario): Promise<unknown> {
    if (scenario.format === 'cjs' && version > 0) {
      purgeRequireCache(entry)
    }

    version += 1
    return loadNative(entry, scenario)
  },

  async teardown() {
    hookHandle?.deregister()
    hookHandle = undefined
  },
}

function isFixtureFileUrl(url: string): boolean {
  if (!url.startsWith('file:')) return false
  return stripSearchAndHash(url).startsWith(fixturesUrlPrefix)
}

function stripSearchAndHash(url: string): string {
  const parsed = new URL(url)
  parsed.search = ''
  parsed.hash = ''
  return pathToFileURL(fileURLToPath(parsed)).href
}
