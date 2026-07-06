# AGENTS.md

This repository builds an native addon for accessing Node's
internal `requireBuiltin()` through runtime probing.

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
packages/entry/       Published main package and native addon source.
packages/loader/      Shared JS loader used by the main and platform packages.
packages/<platform>/  Published optional prebuild packages.
scripts/              Build, header preparation, and test orchestration.
test/                 Node-based behavioral tests.
hmr-comparison/       Standalone cache-invalidation comparison harness.
docs/                 Architecture, packaging, release, and support docs.
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

- Package metadata is explicit. Keep `packages/entry/package.json`,
  `packages/<platform>/package.json`, `packages/<platform>/prebuilds.json`, and
  `docs/support-matrix.md` synchronized when the matrix changes.
- Optional package names contain platform only, not backend or ABI.
- Backend and ABI selection happens at runtime inside the JS loader.
- Every loaded native binding must be checked against its exported `backend` and
  `abi`.
- Generated artifacts stay out of git: `build/`, `lib/`, `prebuilt/`,
  `.cache/`, `dist/`, `*.node`, and `*.tsbuildinfo`.

## Documentation

User-facing docs are English by default. Keep README focused on install, usage,
support status, and links. Durable design decisions belong in `docs/rfc/`; the
current implemented architecture belongs in `docs/architecture.md`.
