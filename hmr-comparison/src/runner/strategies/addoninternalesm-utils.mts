import crypto from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import { createRequire } from 'node:module'
import Module from 'node:module'
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
const addonRoot = path.resolve(
  comparisonRoot,
  '..',
  'packages',
  'internal-loader',
  'entry',
)
const nativeCacheRoot = path.join(os.tmpdir(), 'narb-hmr-native-cache')
const hmrNativeCachePackageName = '@esplus/node-addon-internal-loader-hmr-comparison'
const hmrNativeCacheVersion = '0.0.0'

let restoreNativeCacheLoader: (() => void) | undefined

export function purgeFixtureEsmLoadCacheViaAddon(prefix: string): void {
  const loadCache = getAddonEsmLoadCache()
  purgeLoadCacheByPrefix(loadCache, prefix)
}

export function installAddonNativeCacheLoader(): void {
  if (restoreNativeCacheLoader) return

  const moduleWithLoad = Module as typeof Module & {
    _load(request: string, parent: NodeJS.Module | null, isMain: boolean): unknown
  }
  const originalLoad = moduleWithLoad._load

  moduleWithLoad._load = function cachedNativeLoad(request, parent, isMain) {
    if (request.endsWith('.node') && path.isAbsolute(request)) {
      return originalLoad.call(this, materializeNativeAddon(request), parent, isMain)
    }
    return originalLoad.call(this, request, parent, isMain)
  }

  restoreNativeCacheLoader = () => {
    moduleWithLoad._load = originalLoad
    restoreNativeCacheLoader = undefined
  }
}

export function uninstallAddonNativeCacheLoader(): void {
  restoreNativeCacheLoader?.()
}

function getAddonEsmLoadCache(): LoadCache {
  const addon = requireFromStrategy(addonRoot) as RequireBuiltinAddon
  const loaderModule = addon.requireBuiltin('internal/modules/esm/loader') as EsmLoaderModule

  return loaderModule.getOrInitializeCascadedLoader().loadCache
}

function materializeNativeAddon(source: string): string {
  try {
    const data = fs.readFileSync(source)
    const digest = crypto.createHash('sha256').update(data).digest('hex')
    const filename = path.basename(source)
    const destinationDir = path.join(
      nativeCacheRoot,
      hmrNativeCachePackageName.replace(/^@/, '').replace(/[\\/]/g, '-'),
      hmrNativeCacheVersion,
      filename,
      digest,
    )
    const destination = path.join(destinationDir, filename)

    if (fs.existsSync(destination)) {
      return cachedFileMatches(destination, digest) ? destination : source
    }

    fs.mkdirSync(destinationDir, { recursive: true })
    const temp = path.join(
      destinationDir,
      `.${filename}.${process.pid}.${Date.now()}.${Math.random().toString(16).slice(2)}.tmp`,
    )
    fs.writeFileSync(temp, data, { mode: fs.statSync(source).mode })

    try {
      fs.linkSync(temp, destination)
      fs.rmSync(temp, { force: true })
      return destination
    } catch {
      if (fs.existsSync(destination)) {
        fs.rmSync(temp, { force: true })
        return cachedFileMatches(destination, digest) ? destination : source
      }

      try {
        fs.renameSync(temp, destination)
        return cachedFileMatches(destination, digest) ? destination : source
      } catch {
        if (fs.existsSync(destination)) {
          fs.rmSync(temp, { force: true })
          return cachedFileMatches(destination, digest) ? destination : source
        }
        fs.rmSync(temp, { force: true })
        return source
      }
    }
  } catch {
    return source
  }
}

function cachedFileMatches(destination: string, expectedDigest: string): boolean {
  try {
    const actualDigest = crypto.createHash('sha256').update(fs.readFileSync(destination)).digest('hex')
    return actualDigest === expectedDigest
  } catch {
    return false
  }
}
