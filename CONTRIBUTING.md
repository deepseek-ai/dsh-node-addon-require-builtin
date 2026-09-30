# Contributing

## Setup

```sh
corepack enable
pnpm install
pnpm build
pnpm test
```

Node 20 or newer is required. The project uses pnpm workspaces and CommonJS
runtime entrypoints.

## Development Checks

Run these before opening a pull request:

```sh
pnpm typecheck
pnpm test
pnpm test:optional
```

Run backend comparison when changing runtime probing, package loading, or
diagnostics:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

`pnpm test:all` is a local multi-runtime check. It discovers Node 20, 22, 24,
and 26 installations from nvm or from `NODE_TEST_BINS`.

## Reporting Issues

Include the Node version, platform, libc if on Linux, selected backend, and the
full `probe()` diagnostics object when reporting runtime support problems.
