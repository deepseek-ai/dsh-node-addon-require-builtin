# Release

This project is pre-1.0. Treat this as a release checklist, not a stability
policy.

## Versioning

Update the version in:

- `packages/entry/package.json`
- `packages/loader/package.json`
- every published platform package `package.json`

Keep `workspace:*` dependencies in source. pnpm converts them to concrete
versions during pack/publish.

## Preflight

```sh
pnpm install --frozen-lockfile
pnpm typecheck
pnpm test
pnpm test:optional
```

For backend parity:

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm test:backends
```

## Build Current Platform Prebuilds

```sh
eval "$(pnpm -s headers -- --version 24.18.0)"
pnpm build:prebuilds
pnpm test:optional
pnpm test:optional:nodeabi
```

Only publish platform packages with the expected `prebuilt/` contents for that
platform.

## Pack Check

```sh
tmpdir="$(mktemp -d)"
pnpm --dir packages/entry pack --pack-destination "$tmpdir"
tar -xOf "$tmpdir"/*.tgz package/package.json
```

Confirm the package metadata has public package names, concrete dependency
versions, no local registry, and no generated files outside the intended `files`
list.

## Publish

Use npm's default public registry unless a release explicitly targets another
registry:

```sh
pnpm -r publish --access public --no-git-checks
```

Do not commit `.npmrc` files with tokens or registry overrides.
