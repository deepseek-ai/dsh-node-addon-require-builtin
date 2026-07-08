# Release

This project is pre-1.0. Treat this as a release checklist, not a stability
policy.

## Versioning

Use the release bump helper:

```sh
pnpm release:bump patch
pnpm release:bump minor
pnpm release:bump major
pnpm release:bump 0.1.0
```

The helper updates the root workspace package, `hmr-comparison`, and every
published package under `packages/` to one version, refreshes the lockfile with
`--ignore-scripts --lockfile-only`, and runs `release:verify`.

Keep `workspace:*` dependencies in source. pnpm converts them to concrete
versions during pack/publish.

Version bumps are normal source changes. Open a release PR or commit that
updates package versions and the lockfile first, merge it, then create a
matching `vX.Y.Z` tag from that commit. The publish workflow validates that the
tag version matches every published package version.

Example:

```sh
pnpm release:bump patch
git add package.json hmr-comparison/package.json packages/*/package.json pnpm-lock.yaml
git commit -m "Release 0.0.2"
git tag v0.0.2
```

To bump, stage, and commit in one command:

```sh
pnpm release:commit patch
git tag v0.0.2
```

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

Also verify that the packed tarballs install cleanly with lifecycle scripts
enabled:

```sh
node ./scripts/pack-release.mjs "$tmpdir"
node ./scripts/verify-packed-install.mjs "$tmpdir"
```

## Publish

Use GitHub Actions for release builds so every native binary is built on its
matching platform. The `Release` workflow is manual:

1. Run it with `publish=false` to build all platform prebuilds, assemble package
   tarballs, and upload the `npm-tarballs` artifact for inspection.
2. Create and push a `vX.Y.Z` tag that matches the package versions.
3. Run the same workflow from that tag with `publish=true`.

The workflow builds the full set declared by every `packages/<platform>/prebuilds.json`,
packs tarballs in publish order, then publishes only from the final tarballs.
It does not reuse normal CI artifacts for publishing.

Use npm's default public registry unless a release explicitly targets another
registry. The workflow supports npm trusted publishing through GitHub OIDC; if
trusted publishing is not configured, provide an `NPM_TOKEN` repository or
environment secret.

Manual local publish fallback:

```sh
pnpm -r publish --access restricted --no-git-checks
```

Do not commit `.npmrc` files with tokens or registry overrides.
