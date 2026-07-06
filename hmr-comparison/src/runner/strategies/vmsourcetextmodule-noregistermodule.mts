import { createRequire } from 'node:module'

import { createVmSourceTextModuleStrategy } from './vmsourcetextmodule.mjs'

type EsmUtils = {
  registerModule(referrer: unknown, registry: unknown): void
}

const require = createRequire(import.meta.url)

let esmUtils: EsmUtils | undefined
let originalRegisterModule: EsmUtils['registerModule'] | undefined

export const vmSourceTextModuleNoRegisterModuleStrategy = createVmSourceTextModuleStrategy({
  name: 'vmsourcetextmodule-noregistermodule',
  nodeArgs: ['--expose-gc', '--experimental-vm-modules', '--expose-internals'],

  setup() {
    esmUtils = require('internal/modules/esm/utils') as EsmUtils
    originalRegisterModule = esmUtils.registerModule
    esmUtils.registerModule = () => {}
  },

  teardown() {
    if (esmUtils && originalRegisterModule) {
      esmUtils.registerModule = originalRegisterModule
    }
    esmUtils = undefined
    originalRegisterModule = undefined
  },
})
