---
title: Getting Started
---

Stuck?

-   See the [Frequently asked questions](./faq.md)
-   Ask in `#javascript` on <http://slack.bazel.build>
-   Check for [known issues](https://github.com/aspect-build/rules_js/issues)
-   Pay for support, provided by <https://aspect.dev>.

## Installation

From the release you wish to use:
<https://github.com/aspect-build/rules_js/releases>
copy the WORKSPACE snippet into your `WORKSPACE` file.

## Usage

### Bazel basics

Bazel's `BUILD` or `BUILD.bazel` files are used to declare the dependency graph of your code.
They describe the source files and their dependencies, and declare entry points for programs or tests.
However, they don't say _how to build_ the code, that's the job of Bazel rules.

Because `BUILD` files typically declare a finer-grained dependency graph than `package.json` files, Bazel can be smarter about what to fetch or invalidate for a given build.
For example, Bazel might only need to fetch a single npm package for a simple build,
where you might experience other tools installing the entire `package.json` file.

Authoring BUILD files by hand is a chore, so we recommend using the
[`configure`](https://docs.aspect.build/cli/commands/aspect_configure) command from
[Aspect CLI](https://aspect.build/cli) to automate 80% of this work.

Other recommendations:

-   Put [common flags](https://blog.aspect.dev/bazelrc-flags) in your `.bazelrc` file.
-   Use [Renovate](https://docs.renovatebot.com/) to keep your Bazel dependencies up-to-date.

### Node.js

rules_js depends on rules_nodejs version 5.0 or greater.

Installation is included in the `WORKSPACE` snippet you pasted from the Installation instructions above.

**API docs:**

-   Choosing the version of Node.js:
    <https://bazelbuild.github.io/rules_nodejs/install.html>
-   Rules API: <https://bazelbuild.github.io/rules_nodejs/Core.html>
-   The Node.js toolchain: <https://bazelbuild.github.io/rules_nodejs/Toolchains.html>

### Fetch third-party packages from npm

rules_js accesses npm packages using [pnpm].
pnpm's "virtual store" of packages aligns with Bazel's "external repositories",
and the pnpm "linker" which creates the `node_modules` tree has semantics we can reproduce with Bazel actions.

If your code works with pnpm, then you should expect it works under Bazel as well.
This means that if your issue can be reproduced outside of Bazel, using a reproduction with only pnpm,
then we ask that you fix the issue there, and will close such issues filed on rules_js.

The typical usage is to import an entire `pnpm-lock.yaml` file.
Create such a file if you don't have one. You could install pnpm on your machine, or use `npx` to run it.
We recommend this command, which creates a lockfile with minimal installation needed,
using the identical version of pnpm that Bazel is configured with:

```shell
$ bazel run -- @pnpm//:pnpm --dir $PWD install --lockfile-only
```

Instead of checking in a `pnpm-lock.yaml` file, you could use a `package-lock.json` or `yarn.lock`
file with the `npm_package_lock`/`yarn_lock` attributes of `npm_translate_lock`.
If you do, rules_js will run `pnpm import` to generate a `pnpm-lock.yaml` file on-the-fly.
This is only recommended during migrations; see the notes about these attributes in the [migration guide](https://docs.aspect.build/guides/rules_js_migration).

Next, you'll typically use `npm_translate_lock` to translate the lock file to Starlark, which Bazel extensions understand.
The `WORKSPACE` snippet you pasted above already contains this code.

After `npm_translate_lock`, you have two choices:

1.  `load` from the generated `repositories.bzl` file in `WORKSPACE`, like the `WORKSPACE` snippet does.
    This will cause every Bazel execution to evaluate the `npm_translate_lock`, making it "eager".
    The execution is fast and only invalidated when the `pnpm-lock.yaml` file changes, so we recommend
    this approach.
1.  Check the generated `repositories.bzl` file into your version control, and `load` it from there.
    This fixes the "eager" execution, however it means you need some way to ensure the file stays
    up-to-date as the `pnpm-lock.yaml` file changes. This approach can be useful for bazel rules which
    want to hide their transitive dependencies from users. See https://github.com/bazelbuild/rules_python/issues/608 for a similar discussion about rules_python `pip_parse` which is similar.

Technically, we run a port of pnpm rather than pnpm itself. Here are some design details:

1. You don't need to install pnpm on your machine to build and test with Bazel.
1. We re-use pnpm's resolver, by consuming the `pnpm-lock.yaml` file it produces.
1. We use Bazel's downloader API to fetch package tarballs and extract them to external repositories.
   To modify the URLs Bazel uses to download packages (for example, to fetch from Artifactory), read
   <https://blog.aspect.dev/configuring-bazels-downloader>.
1. We re-use the [`@pnpm/lifecycle`](https://www.npmjs.com/package/@pnpm/lifecycle) package to perform postinstall steps.
   (These run as cacheable Bazel actions.)
1. Finally, you link the `node_modules` tree by adding a `npm_link_package` or `npm_link_all_packages` in your `BUILD` file,
   which populates a tree under `bazel-bin/[path/to/package]/node_modules`.

After importing the lockfile, you should be able to fetch the resulting repository.
Assuming your `npm_translate_lock` was named `npm`, you can run:

```shell
$ bazel fetch @npm//...
```

### Link the node_modules

Next, we'll need to "link" these npm packages into a `node_modules` tree.
If you use [pnpm workspaces], the `node_modules` tree contains first-party packages from your
monorepo as well as third-party packages from npm.

> Bazel doesn't use the `node_modules` installed in your source tree.
> You do not need to run `pnpm install` before running Bazel commands.
> Changes you make to files under `node_modules` in your source tree are not reflected in Bazel results.

Typically, you'll just link all npm packages into the Bazel package containing the `package.json` file.
If you use [pnpm workspaces], you will do this for each npm package in your monorepo.

In `BUILD.bazel`:

```starlark
load("@npm//:defs.bzl", "npm_link_all_packages")

npm_link_all_packages()
```

You can see this working by running `bazel build ...`, then look in the `bazel-bin` folder.

You'll see something like this:

```bash
# the virtual store
bazel-bin/node_modules/.aspect_rules_js
# symlink into the virtual store
bazel-bin/node_modules/some_pkg
# If you used pnpm workspaces:
bazel-bin/packages/some_pkg/node_modules/some_dep
```

**API docs:**

-   [npm_import](./npm_import.md): Import all packages from the pnpm-lock.yaml file, or import individual packages.
-   [npm_link_package](./npm_link_package.md): Link npm package(s) into the `bazel-bin/[path/to/package]/node_modules` tree so that the Node.js runtime can resolve them.

### JavaScript

rules_js provides some primitives to work with JS files.
However, since JavaScript is an interpreted language, simple use cases don't require performing build steps like compilation.

The Node.js module resolution algorithm requires that all files (sources, generated code, and dependencies) be co-located in a common filesystem tree, which is the working directory for the
Node.js interpreter.

As described earlier, the dependencies were linked into `bazel-bin/[path/to/package]/node_modules`,
and Bazel places generated files in `bazel-bin/[path/to/package]`. This leaves source files to be
copied to this location.

> Copying sources to the bazel-bin folder is surprising if you come from a Bazel background, as other
> Bazel rulesets accomodate tooling by teaching it to mix a source folder and an output folder.
> This is not possible with Node.js, without breaking compatibility of many tools.

Our custom rules will take care of copying their sources to the `bazel-bin` output folder automatically.
However this only works when those sources are under the same `BUILD` file as the target that does
the copying. If you have a source file in another `BUILD` file, you'll need to explicitly copy that
with a rule like [`copy_to_bin`](https://docs.aspect.build/aspect-build/bazel-lib/v1.0.0/docs/copy_to_bin-docgen.html#copy_to_bin).

**API docs:**

-   [js_library](./js_library.md): Declare a logical grouping of JS files and their dependencies.
-   [js_binary](./js_binary.md): Declare a Node.js executable program.
-   [js_run_binary](./js_run_binary.md): Run a Node.js executable program as the "tool" in a Bazel action that produces outputs, similar to `genrule`.

### Using binaries published to npm

rules_js automatically mirrors the `bin` field from the `package.json` file of your npm dependencies
to a Starlark API you can load from in your BUILD file or macro.

For example, if you depend on the `typescript` npm package in your root `package.json`, the `tsc` bin entry can be accessed in a `BUILD`:

```starlark=
load("@npm//:typescript/package_json.bzl", typescript_bin = "bin")

typescript_bin.tsc(
    name = "compile",
    srcs = [
        "fs.ts",
        "tsconfig.json",
        "//:node_modules/@types/node",
    ],
    outs = ["fs.js"],
    chdir = package_name(),
    args = ["-p", "tsconfig.json"],
)
```

If you depend on the `typescript` npm package from a nested `package.json` such as `myapp/package.json`, the bin entry would be loaded from the nested package:

```starlark=
load("@npm//myapp:typescript/package_json.bzl", typescript_bin = "bin")
```

Each bin exposes three rules, one for each Bazel command ("verb"): build, test and run - each aligning with the corresponding [js_run_binary](./js_run_binary.md), [js_test](#js_test) and [js_binary](./js_binary.md) rule APIs.

For example:

| Rule         | Underlying Rule | Invoked with  | To              |
| ------------ | --------------- | ------------- | --------------- |
| `foo`        | `js_run_binary` | `bazel build` | produce outputs |
| `foo_binary` | `js_binary`     | `bazel run`   | side-effects    |
| `foo_test`   | `js_test`       | `bazel test`  | assert exit `0` |

> Note: this doesn't cause an eager fetch!
> Bazel doesn't download the typescript package when loading this file, so you can safely write this
> even in a BUILD.bazel file that includes unrelated rules.

To inspect what's in the `@npm` workspace, start with a `bazel query` like the following:

```shell
$ bazel query @npm//... --output=location | grep bzl_library
/shared/cache/bazel/user_base/581b2ac03dd093577e8a6ba6b6509be5/external/npm/BUILD.bazel:5095:12: bzl_library rule @npm//:typescript_bzl_library
/shared/cache/bazel/user_base/581b2ac03dd093577e8a6ba6b6509be5/external/npm/examples/macro/BUILD.bazel:4:12: bzl_library rule @npm//examples/macro:mocha_bzl_library
```

This shows locations on disk where the npm packages can be loaded.

To see the definition of one of these targets, you can run another `bazel query`:

```shell
$ bazel query --output=build @npm//:typescript_bzl_library
# /shared/cache/bazel/user_base/581b2ac03dd093577e8a6ba6b6509be5/external/npm/BUILD.bazel:5095:12
bzl_library(
  name = "typescript_bzl_library",
  visibility = ["//visibility:public"],
  srcs = ["@npm//:typescript/package_json.bzl"],
  deps = ["@npm__typescript__4.9.5//:typescript_bzl_library"],
)
```

This shows us that the label `@npm//:typescript/package_json.bzl` can be used to load the "bin" symbol. You can also follow the location on disk to find that file.

### Macros

[Bazel macros] are a critical part of making your BUILD files more maintainable.
Make sure to follow the [Style Guide](https://bazel.build/rules/bzl-style#macros) when writing a macro,
since some anti-patterns can make your BUILD files difficult to change in the future.

Like Custom Rules, Macros require you to use the Starlark language, but writing a macro is much easier
since it merely composes existing rules together, rather than writing any from scratch.
We believe that most use cases can be accomplished with macros, and discourage you learning how to write
custom rules unless you're really interested in investing time becoming a Bazel expert.

You can think of Macros as a way to create your own Build System, by piping the existing tools together
(like a unix pipeline that composes command-line utilities by piping their stdout/stdin).

As an example, we could write a wrapper for the `typescript_bin.tsc` rule above.

In `tsc.bzl` we could write:

```starlark
load("@npm//:typescript/package_json.bzl", typescript_bin = "bin")

def tsc(name, args = ["-p", "tsconfig.json"], **kwargs):
    typescript_bin.tsc(
        name = name,
        args = args,
        # Always run tsc with the working directory in the project folder
        chdir = native.package_name(),
        **kwargs
    )
```

so that the users `BUILD` file can omit some of the syntax and default settings:

```starlark
load(":tsc.bzl", "tsc")

tsc(
    name = "two",
    srcs = [
        "tsconfig.json",
        "two.ts",
        "//:node_modules/@types/node",
        "//examples/js_library/one",
    ],
    outs = [
        "two.js",
    ],
)
```

### Custom rules

If macros are not sufficient to express your Bazel logic, you can use a custom rule instead.
Aspect has written a number of these based on rules_js, such as:

-   [rules_ts](https://github.com/aspect-build/rules_ts) - Bazel rules for the `tsc` compiler from <http://typescriptlang.org>
-   [rules_swc](https://github.com/aspect-build/rules_swc) - Bazel rules for the swc toolchain <https://swc.rs/>
-   [rules_jest](https://github.com/aspect-build/rules_jest) - Bazel rules to run tests using https://jestjs.io
-   [rules_esbuild](https://github.com/aspect-build/rules_esbuild) - Bazel rules for <https://esbuild.github.io/> JS bundler
-   [rules_webpack](https://github.com/aspect-build/rules_webpack) - Bazel rules for webpack bundler <https://webpack.js.org/>
-   [rules_terser](https://github.com/aspect-build/rules_terser) - Bazel rules for <https://terser.org/> - a JavaScript minifier
-   [rules_rollup](https://github.com/aspect-build/rules_rollup) - Bazel rules for <https://rollupjs.org/> - a JavaScript bundler
-   [rules_deno](https://github.com/aspect-build/rules_deno) - Bazel rules for Deno http://deno.land

You can also write your own custom rule, though this is an advanced topic and not covered in this documentation.

### Documenting your macros and custom rules

You can use [stardoc] to produce API documentation from Starlark code.
We recommend producing Markdown output, and checking those `.md` files into your source repository.
This makes it easy to browse them at the same revision as the sources.

You'll need to create `bzl_library` targets for your Starlark files.
This is a good practice as it lets users of your code generate their own documentation as well.

In addition, Aspect's bazel-lib provides some helpers that make it easy to run stardoc and check that it's always up-to-date.

Continuing our example, where we wrote a macro in `tsc.bzl`, we'd write this to document it, in `BUILD`:

```starlark
load("@aspect_bazel_lib//lib:docs.bzl", "stardoc_with_diff_test", "update_docs")
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "tsc",
    srcs = ["tsc.bzl"],
    deps = [
        # this is a bzl_library target, exposing the package_json.bzl file we depend on
        "@npm//:typescript",
    ],
)

stardoc_with_diff_test(
    name = "tsc-docs",
    bzl_library_target = ":tsc",
)

update_docs(name = "docs")
```

This setup appears in [examples/macro](https://github.com/aspect-build/rules_js/blob/main/examples/macro/BUILD.bazel).

### Create first-party npm packages

You can declare an npm package from sources in your repository.

The package can be exported for usage outside the repository, to a registry like npm or Artifactory.
Or, you can use it locally within a monorepo using [pnpm workspaces].

> Note: we don't yet document how to publish. For now, build the `npm_package` target with `bazel build`, then
> `cd` into the `bazel-out` folder where the package was created, and run `npm pack` or `npm publish`.

**API docs:**

-   [npm_package](./npm_package.md)

[pnpm]: https://pnpm.io/
[pnpm workspaces]: https://pnpm.io/workspaces
[bazel macros]: https://bazel.build/rules/macros
[gazelle]: https://github.com/bazelbuild/bazel-gazelle
[stardoc]: https://github.com/bazelbuild/stardoc

### Debugging

Add [Debug options](https://docs.aspect.build/guides/bazelrc#debug-options) and [Options for JavaScript](https://docs.aspect.build/guides/bazelrc#options-for-javascript) to your project’s .bazelrc file to add the `--config=debug` settings for debugging Node.js programs.

In this repository, for example, we can debug the `//examples/js_binary:test_test` `js_test` target with,

```
$ bazel run //examples/js_binary:test_test --config=debug
Starting local Bazel server and connecting to it...
INFO: Analyzed target //examples/js_binary:test_test (65 packages loaded, 1023 targets configured).
INFO: Found 1 target...
Target //examples/js_binary:test_test up-to-date:
  bazel-bin/examples/js_binary/test_test.sh
INFO: Elapsed time: 6.774s, Critical Path: 0.08s
INFO: 6 processes: 4 internal, 2 local.
INFO: Build completed successfully, 6 total actions
INFO: Build completed successfully, 6 total actions
exec ${PAGER:-/usr/bin/less} "$0" || exit 1
Executing tests from //examples/js_binary:test_test
-----------------------------------------------------------------------------
Debugger listening on ws://127.0.0.1:9229/76b4bb42-7d4e-41f6-a7fe-92b57db356ad
For help, see: https://nodejs.org/en/docs/inspector
```

#### Debugging with Chrome DevTools

At this point you can connect to this Node.js debugging session with a debugging tool.
To use Chrome, open a new tab and enter the URL `chrome://inspect/`. You should see the
session listed there and you can connect to it and debug in Chrome DevTools.
See [Debugging Node.js with Chrome DevTools](https://medium.com/@paul_irish/debugging-node-js-nightlies-with-chrome-devtools-7c4a1b95ae27)
to understand the basics of using the DevTools with Node.

#### Debugging with Visual Studio Code

In this repository, we have added a VSCode the `.vscode/launch.json` configuration file
so you can launch into a debugging session directly from the
[Run & Debug](https://code.visualstudio.com/docs/editor/debugging) window.

## Troubleshooting

See [docs/troubleshooting.md](docs/troubleshooting.md).
