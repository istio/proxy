# Bazel rules for JavaScript

This ruleset is a high-performance and npm-compatible Bazel integration for JavaScript.

-   Lazy: only fetches/installs npm packages needed for the requested build/test targets.
-   Correct: works seamlessly with node.js module resolution. For example there are no pathMapping issues with TypeScript `rootDirs`.
-   Fast: Bazel's sandbox only sees npm packages as directories, not individual files.
-   Supports npm "workspaces": nested npm packages in a monorepo.

Many companies are successfully building with rules_js. If you're getting value from the project, please let us know! Just comment on our [Adoption Discussion](https://github.com/aspect-build/rules_js/discussions/1000).

<https://blog.aspect.dev/rulesjs-npm-benchmarks> shows benchmarks for fetching, installing, and linking packages under rules_js as well as typical alternatives like npm and yarn.

Google does not fund development of rules_js. If your company benefits, please consider donating to continue development and maintenance work: <https://opencollective.com/aspect-build/projects/rules_js>

rules_js is just a part of what Aspect provides:

-   _Need help?_
    -   Community support is available on the #javascript channel on [Bazel Slack](https://slack.bazel.build/)
    -   This ruleset has commercial support provided by https://aspect.dev.
-   See our other Bazel rules, especially those built for rules_js:
    -   [rules_ts](https://github.com/aspect-build/rules_ts) - Bazel rules for [TypeScript](http://typescriptlang.org)
    -   [rules_swc](https://github.com/aspect-build/rules_swc) - Bazel rules for [swc](https://swc.rs)
    -   [rules_jest](https://github.com/aspect-build/rules_jest) - Bazel rules to run tests using [Jest](https://jestjs.io)
    -   [rules_esbuild](https://github.com/aspect-build/rules_esbuild) - Bazel rules for [esbuild](https://esbuild.github.io) JS bundler
    -   [rules_webpack](https://github.com/aspect-build/rules_webpack) - Bazel rules for [Webpack](https://webpack.js.org)
    -   [rules_rollup](https://github.com/aspect-build/rules_rollup) - Bazel rules for [Rollup](https://rollupjs.org) - a JavaScript bundler
    -   [rules_jasmine](https://github.com/aspect-build/rules_jasmine) - Bazel rules to run tests using [Jasmine](https://jasmine.github.io/)
    -   [rules_terser](https://github.com/aspect-build/rules_terser) - Bazel rules for [Terser](https://terser.org) - a JavaScript minifier
    -   [rules_cypress](https://github.com/aspect-build/rules_cypress) - Bazel rules to run tests using [Cypress](https://cypress.io)

## Bazel compatibility

The ruleset is known to work with:

-   Bazel 7 using WORKSPACE and Bzlmod _(tested on CI)_
-   Bazel 6 using WORKSPACE and Bzlmod _(tested on CI)_
-   Bazel 5 using WORKSPACE _(no longer tested on CI)_

> [!NOTE]
> Remote Execution (RBE) requires at least Bazel [6.0](https://blog.bazel.build/2022/12/19/bazel-6.0.html).

## Known issues

-   ESM imports escape the runfiles tree and the sandbox due to https://github.com/aspect-build/rules_js/issues/362

## Installation

From the release you wish to use:
<https://github.com/aspect-build/rules_js/releases>
copy the WORKSPACE snippet into your `WORKSPACE` file.

To use a commit rather than a release, you can point at any SHA of the repo.

For example to use commit `abc123`:

1. Replace `url = "https://github.com/aspect-build/rules_js/releases/download/v0.1.0/rules_js-v0.1.0.tar.gz"`
   with a GitHub-provided source archive like
   `url = "https://github.com/aspect-build/rules_js/archive/abc123.tar.gz"`
1. Replace `strip_prefix = "rules_js-0.1.0"` with `strip_prefix = "rules_js-abc123"`
1. Update the `sha256`. The easiest way to do this is to comment out the line, then Bazel will
   print a message with the correct value.

> Note that GitHub source archives don't have a strong guarantee on the sha256 stability, see
> <https://github.blog/2023-02-21-update-on-the-future-stability-of-source-code-archives-and-hashes>

## Usage

See the documentation in the [docs](docs/) folder.

## Examples

Basic usage examples can be found under the [examples](https://github.com/aspect-build/rules_js/tree/main/examples) folder.

> Note that the examples also rely on code in the `/WORKSPACE` file in the root of this repo.

The [e2e](https://github.com/aspect-build/rules_js/tree/main/e2e) folder also has a few useful examples such as [js_image_layer](https://github.com/aspect-build/rules_js/tree/main/e2e/js_image_oci) for containerizing a js_binary and [js_run_devserver](https://github.com/aspect-build/rules_js/tree/main/e2e/js_run_devserver), a generic rule for running a devserver in watch mode with [ibazel](https://github.com/bazelbuild/bazel-watcher).

Larger examples can be found in our [bazel-examples](https://github.com/aspect-build/bazel-examples) repository including:

-   [Next.js](https://github.com/aspect-build/bazel-examples/tree/main/next.js) / [rules_ts](https://github.com/aspect-build/rules_ts)
-   [Angular (cli/architect)](https://github.com/aspect-build/bazel-examples/tree/main/angular)
-   [Angular (ngc)](https://github.com/aspect-build/bazel-examples/tree/main/angular-ngc) / [rules_ts](https://github.com/aspect-build/rules_ts)
-   [React (create-react-app)](https://github.com/aspect-build/bazel-examples/tree/main/react-cra)
-   [Vue](https://github.com/aspect-build/bazel-examples/tree/main/vue)
-   [Jest](https://github.com/aspect-build/bazel-examples/tree/main/jest) / [rules_jest](https://github.com/aspect-build/rules_jest)
-   [NestJS](https://github.com/aspect-build/bazel-examples/tree/main/nestjs) / [rules_ts](https://github.com/aspect-build/rules_ts), [rules_swc](https://github.com/aspect-build/rules_swc)

## Relationship to rules_nodejs

rules_js is an alternative to the `build_bazel_rules_nodejs` Bazel module and
accompanying npm packages hosted in https://github.com/bazelbuild/rules_nodejs,
which is now unmaintained. All users are recommended to use rules_js instead.

rules_js replaces some parts of [bazelbuild/rules_nodejs](http://github.com/bazelbuild/rules_nodejs) and re-uses other parts:

| Layer                           | Legacy                        | Modern                  |
| ------------------------------- | ----------------------------- | ----------------------- |
| Custom rules                    | `npm:@bazel/typescript`, etc. | `aspect_rules_ts`, etc. |
| Package manager and Basic rules | `build_bazel_rules_nodejs`    | `aspect_rules_js`       |
| Toolchain and core providers    | `rules_nodejs`                | `rules_nodejs`          |

The common layer here is the `rules_nodejs` Bazel module, documented as the "core" in
https://bazelbuild.github.io/rules_nodejs/:

> It is currently useful for Bazel Rules developers who want to make their own JavaScript support.

That's what `rules_js` does! It's a completely different approach to making JS tooling work under Bazel.

First, there's dependency management.

-   `build_bazel_rules_nodejs` uses existing package managers by calling `npm install` or `yarn install` on a whole `package.json`.
-   `rules_js` uses Bazel's downloader to fetch only the packages needed for the requested targets, then mimics [`pnpm`](https://pnpm.io/) to lay out a `node_modules` tree.

Then, there's how a Node.js tool can be executed:

-   `build_bazel_rules_nodejs` follows the Bazel idiom: sources in one folder, outputs in another.
-   `rules_js` follows the npm idiom: sources and outputs together in a common folder.

There are trade-offs involved here, but we think the `rules_js` approach is superior for all users,
especially those at large scale. Read below for more in-depth discussion of the design differences
and trade-offs you should be aware of.
Also see the [slides for our Bazel eXchange talk](https://hackmd.io/@aspect/rules_js)

## Design

The authors of `rules_js` spent four years writing and re-writing `build_bazel_rules_nodejs`.
We learned a lot from that project, as well as from discussions with [Rush](https://rushjs.io/) maintainer [@octogonz](https://github.com/octogonz).

There are two core problems:

-   How do you install third-party dependencies?
-   How does a running Node.js program resolve those dependencies?

And there's a fundamental trade-off: make it fast and deterministic, or support 100% of existing use cases.

Over the years we tried a number of solutions and each end of the trade-off spectrum.

### Installing third-party libraries

Downloading packages should be Bazel's job. It has a full featured remote downloader, with a content-address-cached (confusingly called the "repository cache"). We now mirror pnpm's lock file
into starlark code, then use only Bazel repository rules to perform fetches and translate the
dependency graph into Bazel's representation.

For historical context, we started thinking about this in February 2021 in a (now outdated) [design doc](https://hackmd.io/gu2Nj0TKS068LKAf8KanuA)
and have been working through the details since then.

### Running Node.js programs

Fundamentally, Bazel operates out of a different filesystem layout than Node.
Bazel keeps outputs in a distinct tree outside of the sources.

Our first attempt was based on what Yarn PnP and Google-internal Node.js rules do:
monkey-patch the implementation of `require` in NodeJS itself,
so that every resolution can be aware of the source/output tree difference.
The main downside to this is compatibility: many packages on npm make their own assumptions about
how to resolve dependencies without asking the `require` implementation, and you can't patch them all.
Unlike Google, most of us don't want to re-write all the npm packages we use to be compatible.

Our second attempt was essentially to run `npm link` before running a program, using a runtime linker.
This was largely successful at papering over the filesystem layout differences without disrupting
execution of programs. However, it required a lot of workarounds anytime a JS tool wanted to be
aware of the input and output locations on disk. For example, many tools like react-scripts (the
build system used by Create React App aka. CRA) insist on writing their outputs relative to the
working directory. Such programs were forced to be run with Bazel's output folder as the working
directory, and their sources copied to that location.

`rules_js` takes a better approach, where we follow that react-scripts-prompted workaround to the
extreme. We _always_ run JS tools with the working directory in Bazel's output tree.
We can use a `pnpm`-style layout tool to create a `node_modules` under `bazel-out`, and all resolutions
naturally work.

This third approach has trade-offs.

-   The benefit is that very intractable problems like TypeScript's `rootDirs` just go away.
    In that example, we filed https://github.com/microsoft/TypeScript/issues/37378 but it probably
    won't be solved, so many users trip over issues like
    [this](https://github.com/bazelbuild/rules_nodejs/issues/3423) and
    [this](https://github.com/bazelbuild/rules_nodejs/issues/3421). Now this just works, plus results like sourcemaps look like users expect: just like they would if the tool had written outputs in the source tree.
-   The downside is that Bazel rules/macro authors (even `genrule` authors) must re-path
    inputs and outputs to account for the working directory under `bazel-out`,
    and must ensure that sources are copied there first.
    This forces users to pass a `BAZEL_BINDIR` in the environment of every node action.
    https://github.com/bazelbuild/bazel/issues/15470 suggests a way to improve that, avoiding that imposition on users.
