# JavaScript rules for Bazel

## Maintenance Update

As of [this announcement](https://blog.aspect.dev/rulesjs-launch),
the maintainers of rules_nodejs have all moved to a faster, more compatible alternative:
[rules_js](https://github.com/aspect-build/rules_js/).

`rules_js` has a dependency on the "core" module in rules_nodejs, just supplying the nodejs toolchain.
We'll continue to maintain that code (the /nodejs folder under this repo, essentially).

However, unless some new maintainers want to step up for this repository, the remainder of the code (the build_bazel_rules_nodejs starlark module and npm packages like @bazel/typescript) will be unmaintained and effectively deprecated. (We'll still accept small PRs and push patch/minor releases).

If you want to sign up as a maintainer, DM Alex Eagle or Jason Dobies on [bazel slack](http://slack.bazel.build).

At this point we expect there will never be another major (6.x) of rules_nodejs. If there is, it would include only the core, with the rest of the code removed.



Circle CI | Bazel CI
:---: | :---:
[![CircleCI](https://circleci.com/gh/bazelbuild/rules_nodejs/tree/stable.svg?style=svg)](https://circleci.com/gh/bazelbuild/rules_nodejs/tree/stable) | [![Build status](https://badge.buildkite.com/af1a592b39b11923ef0f523cbb223dd3dbd61629f8bc813c07.svg?branch=stable)](https://buildkite.com/bazel/nodejs-rules-nodejs-postsubmit)

The nodejs rules integrate NodeJS development toolchain and runtime with Bazel.

This toolchain can be used to build applications that target a browser runtime,
so this repo can be thought of as "JavaScript rules for Bazel" as well. (We would call this `rules_javascript` if renames weren't so disruptive.)

This repository is maintained by volunteers in the Bazel community. Neither Google, nor the Bazel team, provides support for the code. However, this repository is part of the test suite used to vet new Bazel releases.

We follow semantic versioning. Patch releases have bugfixes, minor releases have new features. Only major releases (1.x, 2.x) have breaking changes. We support [LTS releases](https://blog.bazel.build/2020/11/10/long-term-support-release.html) of Bazel (starting at 4.x), see `SUPPORTED_BAZEL_VERSIONS` in our `/index.bzl` for the list we test against.

We strive to give you an easy upgrade path when we do introduce a breaking change by documenting a migration path.
If you use code from an `/internal` path, these are not subject to our support policy and may have breaking changes or removals with no warning or migration path.

## Documentation

Comprehensive documentation for installing and using the rules, including generated API docs:
https://bazelbuild.github.io/rules_nodejs/

## Quickstart

This is the fastest way to get started.
See the [installation documentation](https://bazelbuild.github.io/rules_nodejs/install.html) for details and alternative methods, or if you already have a Bazel project and you're adding Node/JavaScript support to it.

```sh
$ npx @bazel/create my_workspace
$ cd my_workspace
```

> The `npx` tool is distributed with node. If you prefer, you can run equivalent commands `npm init @bazel` or `yarn create @bazel`.
> If you've used `@bazel/create` before, you may want to use `npx @bazel/create@latest` to get the most recent version.
> Run without any arguments to see available command-line flags.

## Adopters

Thanks to the following active users!

Open-source repositories:

- Angular: [Angular monorepo](https://github.com/angular/angular), [CLI](https://github.com/angular/angular-cli), [Components](https://github.com/angular/components), [Universal](https://github.com/angular/universal)
- Tensorflow: [tf.js](https://github.com/tensorflow/tfjs) and [tensorboard](https://github.com/tensorflow/tensorboard)
- [Selenium](https://github.com/SeleniumHQ/selenium)
- [tsickle](https://github.com/angular/tsickle)
- [incremental-dom](https://github.com/google/incremental-dom)
- [dataform](https://github.com/dataform-co/dataform)
- [Kubernetes test-infra](https://github.com/kubernetes/test-infra)
- [ts-protoc-gen](https://github.com/improbable-eng/ts-protoc-gen)
- [protoc-gen-ts](https://github.com/thesayyn/protoc-gen-ts)

Organizations:

- [Evertz](https://www.evertz.com)
- [Lucidchart](https://www.lucidchart.com)
- [Webdox](https://www.webdox.cl)
- [WeMaintain](https://www.wemaintain.com)
- [LogiOcean](https://www.logiocean.com)
- [Spica](https://spicaengine.com)
- [Domino Data Lab](https://www.dominodatalab.com/)
- [Cookies](https://cookies.co/)
- [Glean](https://www.glean.com/)
- [FasterCI](https://fasterci.com/)
- [Code Intelligence](https://www.code-intelligence.com/)

Not on this list? [Send a PR](https://github.com/bazelbuild/rules_nodejs/edit/stable/README.md) to add your repo or organization!

## User testimonials

From [Lewis Hemens](https://github.com/lewish) at Dataform:

> At Dataform we manage a number of NPM packages, Webpack builds, Node services and Java pipelines across two separate repositories. This quickly became hard for us to manage, development was painful and and deploying code required a many manual steps. We decided to dive in and migrate our build system entirely to Bazel. This was a gradual transition that one engineer did over the course of about 2 months, during which we had both Bazel and non bazel build processes in place. Once we had fully migrated, we saw many benefits to all parts of our development workflow:
> - Faster CI: we enabled the remote build caching which has reduced our average build time from 30 minutes to 5 (for the entire repository)
> - Improvements to local development: no more random bash scripts that you forget to run, incremental builds reduced to seconds from minutes
> - Simplified deployment processes: we can deploy our code to environments in Kubernetes with just one command that builds and pushes images
> - A monorepo that scales: adding new libraries or packages to our repo became easy, which means we do it more and end up write more modular, shared, maintainable code
> - Developing across machine types: our engineers have both Macbooks and Linux machines, bazel makes it easy to build code across both
> - Developer setup time: New engineers can build all our code with just 3 dependencies - bazel, docker and the JVM. The last engineer to join our team managed to build all our code in < 30 minutes on a brand new, empty laptop

From [Jason Bedard](https://github.com/jbedard) at [Allocadia](https://www.allocadia.com):

> At Allocadia we use Bazel as the primary build system in a monorepo consisting of multiple applications, services and deployments across a range of technologies. Bazel has provided many benefits over previous build systems including:
> - reduced CI pipeline time from 60+ to 5-10 minutes
> - increased build and testing stability
> - improved developer ergonomics such as initial setup, faster more consistent local builds
>
> The use of rules_nodejs has provided these benefits across multiple Angular/TypeScript applications, Karma+Jasmine testing, Rollup, npm packaging, protobuf client/server communication, and a variety of Node.js based tooling.
