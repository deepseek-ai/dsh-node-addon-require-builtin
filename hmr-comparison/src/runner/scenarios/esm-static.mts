import path from 'node:path'

import type { Observation, Scenario } from '../types.mjs'

export const esmStaticScenario: Scenario = {
  name: 'esm-static-import',
  format: 'esm',

  entry(fixturesDir: string): string {
    return path.join(fixturesDir, this.name, 'a.mjs')
  },

  readObservation(loadResult: unknown): Observation {
    const module = loadResult as { value: unknown; payload: object }
    return {
      value: module.value,
      payload: module.payload,
    }
  },
}
