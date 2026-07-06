export type ModuleFormat = 'esm' | 'cjs'
export type YesNo = 'yes' | 'no'
export type Finalized = YesNo | '-'

export interface Observation {
  value: unknown
  payload: object
}

export interface Scenario {
  name: string
  format: ModuleFormat
  entry(fixturesDir: string): string
  readObservation(loadResult: unknown): Observation
}

export interface Strategy {
  name: string
  nodeArgs: string[]
  setup(fixturesDir: string): void | Promise<void>
  load(entry: string, scenario: Scenario): unknown | Promise<unknown>
  teardown(): void | Promise<void>
}

export interface RunnerRow {
  scenario: string
  strategy: string
  changed: YesNo
  finalized: Finalized
  supported: YesNo
}

export interface ResultMessage {
  type: 'result'
  result: RunnerRow
}
