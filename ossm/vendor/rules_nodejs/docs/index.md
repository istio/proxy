---
title: Home
layout: default
toc: true
---

# Bazel JavaScript toolchain

The `@rules_nodejs` Bazel module contains a toolchain that fetches a hermetic node and npm (independent of what's on the developer's machine).
It is currently useful for Bazel Rules developers who want to make their own JavaScript support, and
is maintained by community volunteers from [Aspect](https://aspect.dev).
    - [Install and setup](install.md)
    - [Rules API](Core.md)
    - [Toolchains](Toolchains.md)

## Deprecated modules

> 🚨 `@build_bazel_rules_nodejs` and `@bazel/*` packages are now mostly unmaintained! 🚨
>
> See the Maintenance Update in the [root README](https://github.com/bazel-contrib/rules_nodejs#maintenance-update)

Previously this repository also contained the `@build_bazel_rules_nodejs` module and additional npm packages under the `@bazel` scope on [npm](http://npmjs.com/~bazel).

**There are currently no maintainers of those npm modules.**

If you would like to write a rule outside the scope of the projects we recommend hosting them in your GitHub account or the one of your organization.

## Design

Our goal is to make Bazel be a minimal layering on top of existing npm tooling, and to have maximal compatibility with those tools.

This means you won't find a "Webpack vs. Rollup" debate here. You can run whatever tools you like under Bazel. In fact, we recommend running the same tools you're currently using, so that your Bazel migration only changes one thing at a time.

In many cases, there are trade-offs. We try not to make these decisions for you, so instead of paving one "best" way to do things like many JS tooling options, we provide multiple ways. This increases complexity in understanding and using these rules, but also avoids choosing a wrong "winner". For example, you could install the dependencies yourself, or have Bazel manage its own copy of the dependencies, or have Bazel symlink to the ones in the project.

The JS ecosystem is also full of false equivalence arguments. The first question we often get is "What's better, Webpack or Bazel?".
This is understandable, since most JS tooling is forced to provide a single turn-key experience with an isolated ecosystem of plugins, and humans love a head-to-head competition.
Instead Bazel just orchestrates calling these tools.

