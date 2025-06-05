# How to contribute

## Formatting

Starlark files should be formatted by Buildifier.
We suggest using a pre-commit hook to automate this.
First [install pre-commit](https://pre-commit.com/#installation) (>= v3.2.0),
then run

```shell
pre-commit install
```

Otherwise later tooling on CI may yell at you about formatting/linting violations.

## Updating BUILD files

Some targets are generated from sources.
Currently this is just the `bzl_library` targets.
Run `bazel run //:gazelle` to keep them up to date.

## Using this as a development dependency of other rules

You'll commonly find that you develop in another WORKSPACE, such as
some other ruleset that depends on bazel_lib, or in a nested
WORKSPACE in the integration_tests folder.

To always tell Bazel to use this directory rather than some release
artifact or a version fetched from the internet, run this from this
directory:

```sh
OVERRIDE="--override_repository=bazel_lib=$(pwd)/bazel_lib"
echo "build $OVERRIDE" >> ~/.bazelrc
echo "query $OVERRIDE" >> ~/.bazelrc
```

This means that any usage of `@aspect_bazel_lib` on your system will point to this folder.

## Releasing

1. Make sure your git state is at the right place (something like `git fetch; git checkout origin/main`)
1. Determine the next release version, following semver (could automate in the future from changelog)
1. `git tag -a v1.2.3` (will open an editor to put release notes)
1. `git push --tags`
1. Watch the automation run on GitHub actions
1. Update the release page with auto-generated release notes
