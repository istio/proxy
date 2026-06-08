# JavaScript rules for Bazel

[![Build status](https://badge.buildkite.com/af1a592b39b11923ef0f523cbb223dd3dbd61629f8bc813c07.svg?branch=stable)](https://buildkite.com/bazel/nodejs-rules-nodejs-postsubmit)
![GitHub release (latest by date)](https://img.shields.io/github/downloads/bazel-contrib/rules_nodejs/latest/total)

This ruleset provides a Node.js development toolchain and runtime with Bazel.
It does not have any rules for using Node.js, such as `nodejs_binary`.
For that, we recommend [rules_js](https://github.com/aspect-build/rules_js).

This repository is maintained by volunteers in the Bazel community. Neither Google, nor the Bazel team, provides support for the code. However, this repository is part of the test suite used to vet new Bazel releases.

We follow semantic versioning. Patch releases have bugfixes, minor releases have new features. Only major releases (1.x, 2.x) have breaking changes. We support [LTS releases](https://blog.bazel.build/2020/11/10/long-term-support-release.html) of Bazel (starting at 4.x), see `SUPPORTED_BAZEL_VERSIONS` in our `/index.bzl` for the list we test against.

## 6.0 Scope Reduction

This branch is the latest release, 6.x.x
It has a greatly reduced scope from previous releases, as most of the code was unmaintained.
See the 5.x branch for the prior state of the repo.

## Documentation

Comprehensive documentation for installing and using the rules, including generated API docs:
https://bazel-contrib.github.io/rules_nodejs/
