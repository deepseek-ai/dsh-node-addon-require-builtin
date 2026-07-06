import path from 'node:path'

import type { Observation, Scenario } from '../types.mjs'

export const cjsRequireScenario: Scenario = {
  name: 'cjs-require',
  format: 'cjs',

  entry(fixturesDir: string): string {
    return path.join(fixturesDir, this.name, 'a.cjs')
  },

  readObservation(loadResult: unknown): Observation {
    const module = loadResult as { value: unknown; payload: object }
    return {
      value: module.value,
      payload: module.payload,
    }
  },
}
