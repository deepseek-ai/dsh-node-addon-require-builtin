import { fork } from 'node:child_process'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

import { scenarios } from './scenarios/index.mjs'
import { strategies } from './strategies/index.mjs'
import type { ResultMessage, RunnerRow, Scenario, Strategy } from './types.mjs'

type RunnerColumn = keyof RunnerRow
type PrintableRow = Record<RunnerColumn, string>

const PROJECT_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..')
const CASE_WORKER = fileURLToPath(new URL('./case-worker.mjs', import.meta.url))

async function main(): Promise<void> {
  const rows: RunnerRow[] = []

  for (const strategy of strategies) {
    for (const scenario of scenarios) {
      const row = await runCaseInChild({ scenario, strategy })
      rows.push(row)
    }
  }

  printRows(rows)
}

function runCaseInChild({ scenario, strategy }: { scenario: Scenario; strategy: Strategy }): Promise<RunnerRow> {
  return new Promise((resolve, reject) => {
    let result: RunnerRow | undefined
    let stderr = ''

    const child = fork(
      CASE_WORKER,
      ['--scenario', scenario.name, '--strategy', strategy.name],
      {
        cwd: PROJECT_ROOT,
        execArgv: strategy.nodeArgs,
        stdio: ['ignore', 'ignore', 'pipe', 'ipc'],
      },
    )

    child.stderr?.on('data', (chunk: Buffer) => {
      stderr += chunk.toString()
    })

    child.on('message', (message: unknown) => {
      if (isResultMessage(message)) {
        result = message.result
      }
    })

    child.on('error', reject)

    child.on('close', (code, signal) => {
      if (code !== 0) {
        reject(new Error(formatChildFailure({ scenario, strategy, code, signal, stderr })))
        return
      }

      if (!result) {
        reject(new Error(formatChildFailure({ scenario, strategy, code, signal, stderr, missingResult: true })))
        return
      }

      resolve(result)
    })
  })
}

function isResultMessage(message: unknown): message is ResultMessage {
  return typeof message === 'object'
    && message !== null
    && 'type' in message
    && message.type === 'result'
    && 'result' in message
}

function printRows(rows: RunnerRow[]): void {
  const headers: RunnerColumn[] = ['strategy', 'scenario', 'changed', 'finalized', 'supported']
  const widths = Object.fromEntries(
    headers.map((header) => [
      header,
      Math.max(header.length, ...rows.map((row) => String(row[header]).length)),
    ]),
  ) as Record<RunnerColumn, number>

  console.log(formatRow({
    strategy: 'strategy',
    scenario: 'scenario',
    changed: 'changed',
    finalized: 'finalized',
    supported: 'supported',
  }, widths))
  for (const row of rows) {
    console.log(formatRow(toPrintableRow(row), widths))
  }
}

function toPrintableRow(row: RunnerRow): PrintableRow {
  return {
    strategy: row.strategy,
    scenario: row.scenario,
    changed: row.changed,
    finalized: row.finalized,
    supported: row.supported,
  }
}

function formatRow(row: PrintableRow, widths: Record<RunnerColumn, number>): string {
  return Object.entries(widths)
    .map(([key, width]) => String(row[key as RunnerColumn]).padEnd(width))
    .join('  ')
}

function formatChildFailure({
  scenario,
  strategy,
  code,
  signal,
  stderr,
  missingResult = false,
}: {
  scenario: Scenario
  strategy: Strategy
  code: number | null
  signal: NodeJS.Signals | null
  stderr: string
  missingResult?: boolean
}): string {
  const reason = missingResult
    ? 'exited without sending a result'
    : `failed with code ${code}${signal ? ` and signal ${signal}` : ''}`
  const detail = stderr.trim() ? `\n${stderr.trim()}` : ''
  return `Case ${scenario.name}/${strategy.name} ${reason}.${detail}`
}

main().catch((error: unknown) => {
  console.error(error)
  process.exitCode = 1
})
