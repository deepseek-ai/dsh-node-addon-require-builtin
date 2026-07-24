'use strict';

// Libc detection must not consult process.report.getReport() when the running
// node binary carries a readable PT_INTERP: on many-CPU Linux hosts one report
// sweeps every sysfs cpufreq entry live and takes seconds, which turned each
// entry-package require into a multi-second stall. Needs only build:ts.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const loader = require('../packages/loader/lib/index.js');

// --- elfInterpreter parses real and synthetic executables, failing closed ---

const nodeInterpreter = loader.elfInterpreter(process.execPath);
if (process.platform === 'linux') {
  // CI and dev hosts run dynamically linked node builds; a static build would
  // legitimately return undefined, but then the getReport fallback below is
  // exercised instead and this assertion documents the expected environment.
  assert.equal(typeof nodeInterpreter, 'string');
  assert.ok(nodeInterpreter.startsWith('/'), `interpreter should be an absolute path: ${nodeInterpreter}`);
} else {
  assert.equal(nodeInterpreter, undefined);
}

assert.equal(loader.elfInterpreter(path.join(__dirname, '..', 'package.json')), undefined);
assert.equal(loader.elfInterpreter(path.join(__dirname, 'does-not-exist')), undefined);

// Synthetic minimal ELF64 LE images: one dynamic executable per interpreter
// string, one static (no PT_INTERP), one truncated header. 64-byte ELF header,
// then one 56-byte program header, then the interpreter bytes.
function syntheticElf(interpreter) {
  const headerSize = 64;
  const entrySize = 56;
  const interpBytes = interpreter === undefined ? Buffer.alloc(0) : Buffer.from(`${interpreter}\0`);
  const file = Buffer.alloc(headerSize + entrySize + interpBytes.length);
  file.writeUInt32BE(0x7f454c46, 0); // \x7fELF
  file[4] = 2; // ELFCLASS64
  file[5] = 1; // ELFDATA2LSB
  file.writeBigUInt64LE(BigInt(headerSize), 0x20); // e_phoff
  file.writeUInt16LE(entrySize, 0x36); // e_phentsize
  file.writeUInt16LE(1, 0x38); // e_phnum
  const PT_INTERP = 3;
  const PT_LOAD = 1;
  file.writeUInt32LE(interpreter === undefined ? PT_LOAD : PT_INTERP, headerSize);
  file.writeBigUInt64LE(BigInt(headerSize + entrySize), headerSize + 0x08); // p_offset
  file.writeBigUInt64LE(BigInt(interpBytes.length), headerSize + 0x20); // p_filesz
  interpBytes.copy(file, headerSize + entrySize);
  return file;
}

const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'narb-libc-test-'));
try {
  const cases = [
    // name, image, expected interpreter, expected libc family
    ['glibc.elf', syntheticElf('/lib64/ld-linux-x86-64.so.2'), '/lib64/ld-linux-x86-64.so.2', 'glibc'],
    ['musl.elf', syntheticElf('/lib/ld-musl-x86_64.so.1'), '/lib/ld-musl-x86_64.so.1', 'musl'],
    ['static.elf', syntheticElf(undefined), undefined, undefined],
    ['truncated.elf', syntheticElf('/lib64/ld-linux-x86-64.so.2').subarray(0, 32), undefined, undefined],
    // An interpreter name matching neither family classifies as undefined
    // (fail closed) rather than defaulting to either libc.
    ['odd.elf', syntheticElf('/opt/ld-something-else.so'), '/opt/ld-something-else.so', undefined],
  ];
  for (const [name, image, expectedInterp, expectedLibc] of cases) {
    const file = path.join(scratch, name);
    fs.writeFileSync(file, image);
    assert.equal(loader.elfInterpreter(file), expectedInterp, name);
    assert.equal(loader.libcFromExecutable(file), expectedLibc, name);
  }
} finally {
  fs.rmSync(scratch, { recursive: true, force: true });
}

// --- platform suffix resolution never falls back to getReport here ---

// The guard is meaningful on any host where a cheap signal resolves the libc;
// dynamically linked node (CI, dev hosts) always satisfies that on Linux.
if (process.platform !== 'linux' || loader.libcFromExecutable(process.execPath) !== undefined) {
  const originalGetReport = process.report && process.report.getReport;
  if (originalGetReport) {
    process.report.getReport = () => {
      throw new Error('platformPackageSuffix must not consult process.report.getReport()');
    };
  }
  try {
    const suffix = loader.platformPackageSuffix();
    assert.equal(typeof suffix, 'string');
    assert.ok(suffix.length > 0);
    if (process.platform === 'linux') {
      assert.match(suffix, /^linux-.+-(gnu|musl)$/);
    }
  } finally {
    if (originalGetReport) {
      process.report.getReport = originalGetReport;
    }
  }
}

console.log('libc.test.js passed');
