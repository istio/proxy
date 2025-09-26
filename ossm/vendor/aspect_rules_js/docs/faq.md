---
title: FAQ
---

## Flaky build failure: Exec failed due to IOException

Known issue: we sometimes see

```
(00:55:55) ERROR: /mnt/ephemeral/workdir/BUILD.bazel:46:22: Copying directory aspect_rules_js~1.37.0~npm~npm__picocolors__1.0.0/package failed: Exec failed due to IOException: /mnt/ephemeral/output/__main__/execroot/_main/external/aspect_rules_js~1.37.0~npm~npm__picocolors__1.0.0/package (No such file or directory)
```

This is not yet understood, but it seems to be related to "no-remote" execution requirements being dropped from copy actions created by `aspect_bazel_lib`, and likely tied to `remote_download_outputs=minimal|toplevel`.

A workaround is to add to `.bazelrc`:

```
common --modify_execution_info=CopyDirectory=+no-remote,CopyToDirectory=+no-remote,CopyFile=+no-remote
```

> NB: `--modify_execution_info` is NOT additive, see [issue](https://github.com/bazelbuild/bazel/pull/16262)
> This means you must take care to have the flag appear only once.
> Use `--announce_rc` to diagnose where flag values are coming from.

## Why does my program fail with "Module not found"?

See the [Troubleshooting guide](./troubleshooting.md).

## Making the editor happy

Editors (and the language services they host) expect a couple of things:

-   third-party tooling like the TypeScript SDK under `<project root>/node_modules`
-   types for your first-party imports

Since rules_js puts the outputs under Bazel's `bazel-out` tree, the editor doesn't find them by default.

To get local tooling installed, you can continue to run `pnpm install` (or use whatever package manager your lockfile is for)
to get a `node_modules` tree in your project.
If there are many packages to install, you could reduce this by only installing the tooling
actually needed for non-Bazel workflows, like the `@types/*` packages and `typescript`.

To resolve first-party imports like `import '@myorg/my_lib'` to resolve in TypeScript, use the
`paths` key in the `tsconfig.json` file to list additional search locations.
This is the same thing you'd do outside of Bazel.
See [example](https://github.com/aspect-build/rules_ts/blob/74d54bda208695d7e8992520e560166875cfbce7/examples/simple/tsconfig.json#L4-L10).

## Bazel isn't seeing my changes to package.json

rules_js relies on what's in the `pnpm-lock.yaml` file. Make sure your changes are reflected there.

Set `update_pnpm_lock` to True in your `npm_translate_lock` rule and Bazel will auto-update your
`pnpm-lock.yaml` when any of its inputs change. When you do this, add all files required
for pnpm to generate the `pnpm-lock.yaml` to the `data` attribute of `npm_translate_lock`. This will
include the `pnpm-workspace.yaml` if it exists and all `package.json` files in your pnpm workspace.

To list all local `package.json` files that pnpm needs to read, you can run
`pnpm recursive ls --depth -1 --porcelain`.

## Can a tool run outside of Bazel write to the `node_modules` in `bazel-out`?

Some tools such as the AWS SDK write to `node_modules` when they are run. Ideally this should be avoided or fixed in an upstream package. Bazel write-protects the files in the `bazel-out` output tree so they can be reliably cached and reused.

If necessary the `node_modules` directory permissions can be manually modified, however these changes will be detected and overwritten next time Bazel runs. To maintain these edits across Bazel runs, you can use the `--experimental_check_output_files=false` flag.

## Can I edit files in `node_modules` for debugging?

Try running Bazel with `--experimental_check_output_files=false` so that your edits inside the `bazel-out/node_modules` tree are preserved.

## Can I use bazel-managed pnpm?

Yes, just run `bazel run -- @pnpm//:pnpm --dir $PWD` followed by the usual arguments to pnpm.

If you're bootstrapping a new project, you'll need to add this to your WORKSPACE:

```starlark
load("@aspect_rules_js//npm:repositories.bzl", "pnpm_repository")

pnpm_repository(name = "pnpm")
```

Or, if you're using [bzlmod](https://bazel.build/external/overview#bzlmod), add these lines to your MODULE.bazel:

```starlark
pnpm = use_extension("@aspect_rules_js//npm:extensions.bzl", "pnpm", dev_dependency = True)

use_repo(pnpm, "pnpm")
```

This defines the `@pnpm` repository so that you can create the lockfile with
`bazel run -- @pnpm//:pnpm --dir $PWD install --lockfile-only`, and then once the file exists you'll
be able to add the `pnpm_translate_lock` to the `WORKSPACE` which requires the lockfile.

Consider documenting running pnpm through bazel as a good practice for your team, so that all developers run the exact same pnpm and node versions that Bazel does.

## Why can't Bazel fetch an npm package?

If the error looks like this: `failed to fetch. no such package '@npm__foo__1.2.3//': at offset 773, object has duplicate key`
then you are hitting https://github.com/bazelbuild/bazel/issues/15605

The workaround is to patch the package.json of any offending packages in npm_translate_lock, see https://github.com/aspect-build/rules_js/issues/148#issuecomment-1144378565.
Or, if a newer version of the package has fixed the duplicate keys, you could upgrade.

If the error looks like this: `ERR_PNPM_FETCH_404 GET https://registry.npmjs.org/@my-workspace%2Ffoo: Not Found - 404`, where `foo` is a package living in a workspace in your local
codebase and it's being declared [`pnpm-workspace.yaml`](https://pnpm.io/pnpm-workspace_yaml) and that you are relying on the `yarn_lock` attribute of `npm_translate_lock`, then
you're hitting a caveat of the migration process.

The workaround is to generate the `pnpm-lock.yaml` on your own as mentioned in the migration guide and to use the `pnpm_lock` attribute of `npm_translate_lock` instead.

## In my monorepo, can Bazel output multiple packages under one dist/ folder?

Many projects have a structure like the following:

```
my-workspace/
├─ packages/
│  ├─ lib1/
│  └─ lib2/
└─ dist/
   ├─ lib1/
   └─ lib2/
```

However, Bazel has a constraint that outputs for a given Bazel package (a directory containing a `BUILD` file) must be written under the corresponding output folder. This means that you have two choices:

1. **Keep your output structure the same.** This implies there must be a single `BUILD` file under `my-workspace`, since this is the only Bazel package which can output to paths beneath `my-workspace/dist`. The downside is that this `BUILD` file may get long, accumulate a lot of `load` statements, and the paths inside will be longer.

The result looks like this:

```
my-workspace/
├─ BUILD.bazel
├─ packages/
│  ├─ lib1/
│  └─ lib2/
└─ bazel-bin/packages/
   ├─ lib1/
   └─ lib2/
```

2. **Change your output structure** to distribute `dist` folders beneath `lib1` and `lib2`. Now you can have `BUILD` files underneath each library, which is more Bazel-idiomatic.

The result looks like this:

```
my-workspace/
├─ packages/
│  ├─ lib1/
│  |  └─ BUILD.bazel
│  ├─ lib2/
│  |  └─ BUILD.bazel
└─ bazel-bin/packages/
   ├─ lib1/
   |  └─ dist/
   └─ lib2/
      └─ dist/
```

Note that when following option 2, it might require updating some configuration files which refer to the original output locations. For example, your `tsconfig.json` file might have a `paths` section which points to the `../../dist` folder.

To keep your legacy build system working during the migration, you might want to avoid changing those configuration files in-place. For this purpose, you can use [the `jq` rule](https://docs.aspect.build/aspect-build/bazel-lib/v1.0.0/docs/jq-docgen.html#jq) in place of `copy_to_bin`, using a `filter` expression so the copy of the configuration file in `bazel-bin` that's used by the Bazel build can have a different path than the configuration file in the source tree.
