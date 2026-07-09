# AGENTS.md

This repository builds native addons for accessing Node's internal
`requireBuiltin()` through runtime probing.

## Pre-release stance

The project is pre-1.0. Prefer the correct public shape over compatibility
shims. If a package name, exported field, layout, or diagnostic shape is wrong,
rename it and update all references in the same change. Do not add deprecated
aliases unless the project has already published a stable release that needs
them.

## Runtime safety rules

- The addon must fail closed. If a private symbol, image check, machine-code
  pattern, pointer invariant, or smoke test fails, return unsupported or throw a
  clear error. Do not guess offsets.
- Do not include Node source private headers such as `env-inl.h`,
  `node_realm-inl.h`, or `node_api_internals.h`.
- The `nodeabi` backend may use official `node-vX.Y.Z-headers.tar.gz` public
  headers only.
- Keep the public Node-API layer small. Runtime compatibility belongs below the
  API layer in the selected backend and shared probe helper.
- Do not broaden the optional package matrix until the runtime path is
  implemented and CI-validated for that platform/libc/arch.

## Repository layout

```text
packages/native/                     Shared native source, not published.
packages/loader/                     Shared JS loader package.
packages/require-builtin/entry/      Published unrestricted entry package.
packages/require-builtin/<platform>/ Published unrestricted prebuild packages.
packages/internal-loader/entry/      Published whitelisted entry package.
packages/internal-loader/<platform>/ Published whitelisted prebuild packages.
scripts/                             Build, header, release, and test scripts.
test/                                Node-based behavioral tests.
hmr-comparison/                      Cache-invalidation comparison harness.
docs/                                Architecture, packaging, release docs.
```

## Commands

```sh
pnpm install
pnpm build
pnpm test
pnpm typecheck
pnpm test:optional
pnpm test:all
```

`pnpm test:backends` requires official Node.js public headers:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

## Packaging invariants

- Package metadata is explicit. Keep `packages/<family>/entry/package.json`,
  `packages/<family>/<platform>/package.json`,
  `packages/<family>/<platform>/prebuilds.json`, and
  `docs/support-matrix.md` synchronized when the matrix changes.
- Optional package names contain product family and platform only, not backend
  or ABI.
- Backend and ABI selection happens at runtime inside the JS loader.
- Every loaded native binding must be checked against its exported `product`,
  `backend`, and `abi`.
- Generated artifacts stay out of git: `build/`, `lib/`, `prebuilt/`,
  `.cache/`, `dist/`, `*.node`, and `*.tsbuildinfo`.

## Documentation

User-facing docs are English by default. Keep README focused on install, usage,
support status, and links. Durable design decisions belong in `docs/rfc/`; the
current implemented architecture belongs in `docs/architecture.md`.
