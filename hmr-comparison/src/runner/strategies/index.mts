import { addonInternalEsmAndRequirecacheStrategy } from './addoninternalesm-and-requirecache.mjs'
import { addonInternalEsmNativecacheAndRequirecacheStrategy } from './addoninternalesm-nativecache-and-requirecache.mjs'
import { addonInternalEsmNativecacheStrategy } from './addoninternalesm-nativecache.mjs'
import { addonInternalEsmStrategy } from './addoninternalesm.mjs'
import { hookversionAndRequirecacheStrategy } from './hookversion-and-requirecache.mjs'
import { hookversionStrategy } from './hookversion.mjs'
import { internalesmAndRequirecacheStrategy } from './internalesm-and-requirecache.mjs'
import { internalesmStrategy } from './internalesm.mjs'
import { isolatedvmStrategy, isIsolatedVmAvailable } from './isolatedvm.mjs'
import { naiveStrategy } from './naive.mjs'
import { requirecacheStrategy } from './requirecache.mjs'
import { vmcontextStrategy } from './vmcontext.mjs'
import { vmSourceTextModuleNoRegisterModuleStrategy } from './vmsourcetextmodule-noregistermodule.mjs'
import { vmsourcetextmoduleStrategy } from './vmsourcetextmodule.mjs'
import type { Strategy } from '../types.mjs'

export const strategies: Strategy[] = [
  naiveStrategy,
  requirecacheStrategy,
  hookversionStrategy,
  hookversionAndRequirecacheStrategy,
  internalesmStrategy,
  internalesmAndRequirecacheStrategy,
  addonInternalEsmStrategy,
  addonInternalEsmAndRequirecacheStrategy,
  addonInternalEsmNativecacheStrategy,
  addonInternalEsmNativecacheAndRequirecacheStrategy,
  ...(isIsolatedVmAvailable() ? [isolatedvmStrategy] : []),
  vmsourcetextmoduleStrategy,
  vmSourceTextModuleNoRegisterModuleStrategy,
  vmcontextStrategy,
]

export const strategiesByName = new Map<string, Strategy>(
  strategies.map((strategy) => [strategy.name, strategy]),
)
