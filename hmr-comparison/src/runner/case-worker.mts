import path from 'node:path'
import { fileURLToPath } from 'node:url'

import { scenariosByName } from './scenarios/index.mjs'
import { strategiesByName } from './strategies/index.mjs'
import type { Finalized, Observation, ResultMessage, RunnerRow, Scenario, Strategy, YesNo } from './types.mjs'

const LOADS_PER_CASE = 2
const PROJECT_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..')
const FIXTURES_DIR = path.join(PROJECT_ROOT, 'fixtures')

class FinalizationTracker {
  #registry: FinalizationRegistry<{ key: string; sample: number }>
  #samplesByKey = new Map<string, number>()
  #finalizedSamplesByKey = new Map<string, Set<number>>()
  #seenPayloads = new WeakMap<object, number>()

  constructor() {
    this.#registry = new FinalizationRegistry((heldValue) => {
      const finalizedSamples = this.#finalizedSamplesByKey.get(heldValue.key) ?? new Set<number>()
      finalizedSamples.add(heldValue.sample)
      this.#finalizedSamplesByKey.set(heldValue.key, finalizedSamples)
    })
  }

  track(key: string, payload: object): number {
    const seenSample = this.#seenPayloads.get(payload)
    if (seenSample !== undefined) return seenSample

    const sample = (this.#samplesByKey.get(key) ?? 0) + 1
    this.#samplesByKey.set(key, sample)
    this.#seenPayloads.set(payload, sample)
    this.#registry.register(payload, { key, sample })
    return sample
  }

  finalizedSample(key: string, sample: number): boolean {
    return this.#finalizedSamplesByKey.get(key)?.has(sample) ?? false
  }
}

async function main(): Promise<void> {
  if (typeof global.gc !== 'function') {
    throw new Error('Case worker requires node --expose-gc for finalization observation.')
  }

  const scenario = getRequired(scenariosByName, readArg('--scenario'), 'scenario')
  const strategy = getRequired(strategiesByName, readArg('--strategy'), 'strategy')
  const result = await runCase({ scenario, strategy })
  sendResult(result)
}

async function runCase({ scenario, strategy }: { scenario: Scenario; strategy: Strategy }): Promise<RunnerRow> {
  const key = `${scenario.name}:${strategy.name}`
  const tracker = new FinalizationTracker()
  const values: unknown[] = []
  const samples: number[] = []

  try {
    await strategy.setup(FIXTURES_DIR)

    for (let iteration = 0; iteration < LOADS_PER_CASE; iteration += 1) {
      const observation = await loadAndTrack({ scenario, strategy, tracker, key })
      values.push(observation.value)
      samples.push(observation.sample)
    }
  } finally {
    await strategy.teardown()
  }

  await forceGcAndSettle()

  const changed = values.length >= 2 && !Object.is(values[0], values[1])
  const staleSamples = staleSamplesBeforeLatest(samples)
  const finalized: Finalized = changed
    ? yesNo(staleSamples.length > 0
      && staleSamples.every((sample) => tracker.finalizedSample(key, sample)))
    : '-'

  return {
    scenario: scenario.name,
    strategy: strategy.name,
    changed: yesNo(changed),
    finalized,
    supported: yesNo(changed),
  }
}

async function loadAndTrack({
  scenario,
  strategy,
  tracker,
  key,
}: {
  scenario: Scenario
  strategy: Strategy
  tracker: FinalizationTracker
  key: string
}): Promise<{ value: unknown; sample: number }> {
  const entry = scenario.entry(FIXTURES_DIR)
  const loadResult = await strategy.load(entry, scenario)
  const observation: Observation = scenario.readObservation(loadResult)

  return {
    value: observation.value,
    sample: tracker.track(key, observation.payload),
  }
}

function staleSamplesBeforeLatest(values: number[]): number[] {
  if (values.length === 0) return []
  const latest = values.at(-1)
  return [...new Set(values.slice(0, -1))].filter((value) => value !== latest)
}

async function forceGcAndSettle(): Promise<void> {
  for (let i = 0; i < 10; i += 1) {
    global.gc?.()
    await sleep(0)
  }

  await sleep(50)
}

function readArg(name: string): string {
  const index = process.argv.indexOf(name)
  if (index === -1 || index + 1 >= process.argv.length) {
    throw new Error(`Missing required argument: ${name}`)
  }
  return process.argv[index + 1]
}

function getRequired<T>(map: Map<string, T>, name: string, label: string): T {
  const value = map.get(name)
  if (!value) throw new Error(`Unknown ${label}: ${name}`)
  return value
}

function sendResult(result: RunnerRow): void {
  const message: ResultMessage = { type: 'result', result }
  if (typeof process.send === 'function') {
    process.send(message)
  } else {
    console.log(JSON.stringify(result))
  }
}

function yesNo(value: boolean): YesNo {
  return value ? 'yes' : 'no'
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, ms)
  })
}

main().catch((error: unknown) => {
  console.error(error)
  process.exitCode = 1
})
