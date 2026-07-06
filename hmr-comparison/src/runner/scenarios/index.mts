import { cjsRequireScenario } from './cjs-require.mjs'
import { esmStaticScenario } from './esm-static.mjs'
import type { Scenario } from '../types.mjs'

export const scenarios: Scenario[] = [esmStaticScenario, cjsRequireScenario]

export const scenariosByName = new Map<string, Scenario>(
  scenarios.map((scenario) => [scenario.name, scenario]),
)
