# How to Contribute

## Setup

Install bazelisk following https://bazel.build/install/bazelisk
or run `npm i -g @bazel/bazelisk`.

## Formatting

Starlark files should be formatted by buildifier.
We suggest using a pre-commit hook to automate this.
First [install pre-commit](https://pre-commit.com/#installation),
then run

```shell
pre-commit install
```

Otherwise later tooling on CI will yell at you about formatting/linting violations.

## Updating BUILD files

Some targets are generated from sources.
Currently this is just the `bzl_library` targets.
Run `bazel run //:gazelle` to keep them up-to-date.

## Using this as a development dependency of other rules

You'll commonly find that you develop in another WORKSPACE, such as
some other ruleset that depends on `@aspect_rules_js`, or in a nested
WORKSPACE in the integration_tests folder.

To always tell Bazel to use this directory rather than some release
artifact or a version fetched from the internet, run this from this
directory:

```sh
OVERRIDE="--override_repository=aspect_rules_js=$(pwd)/rules_js"
echo "common:override $OVERRIDE" >> ~/.bazelrc
```

This means that any usage of `@aspect_rules_js` on your system will point to this folder, if you
pass the `--config=override` flag to Bazel.
(We don't want to enable this for all builds, since you'll later forget this was done and wonder
why you don't reproduce the same behavior as others.)

## Running tests

Simply run `bazel test //...`

## Releasing

1. Push a tag to the repo, or create one on the GH UI
1. Watch the automation run on GitHub actions
