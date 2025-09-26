# `rules_rust` backwards compatibility policy

This document defines the backwards compatibility policy for rules_rust and
defines the process for making compatibility breaking changes. Any exception to
this process will have to be thoroughly discussed and there will have to be a
consensus between maintainers that the exception should be granted.

## What is a compatibility breaking change?

A backwards compatible change has the property that a build that is green,
correct, is using stable APIs of `rules_rust`, and is using a supported version
of Bazel before the change is still green and correct after the change. In
practice it means that users should not have to modify their project source
files, their `BUILD` files, their `bzl` files, their `rust_toolchain`
definitions, their platform definitions, or their build settings.

A backwards compatible release has the property that it only contains backward
compatible changes.

`rules_rust` follow [SemVer 2.0.0](https://semver.org/). `rules_rust` promise
that all minor version number and patch version number releases only contain
backwards compatible changes.

`rules_rust` don't make any guarantees about compatibility between a released
version and the state of the `rules_rust` at HEAD.

Backwards incompatible changes will have to follow a process in order to be
released. Users will be given at least one release where the old behavior is
still present but deprecated to allow smooth migration of their project to the
new behavior.

### Compatibility before 1.0

All minor version number releases before version 1.0 can be backwards incompatible.
Backwards incompatible changes still have to follow the the process, but minimum
time for migration is reduced to 2 weeks.

## What Bazel versions are supported?

`rules_rust` support the current Bazel LTS at the time of a `rules_rust`
release.

Support for the current Bazel rolling release is on the best effort basis. If
the CI build of `rules_rust` against the current Bazel rolling release is green,
it has to stay green. If the build is already red (because the new Bazel rolling
release had incompatible changes that broke `rules_rust`), it is acceptable to
merge PRs leaving the build red as long as the reason for failure remains the
same. We hope red CI with the current rolling release will be rare.

`rules_rust` don't promise all new `rules_rust` features available to the
current Bazel rolling release to be be available to the current Bazel LTS
release (because Bazel compatibility policy doesn't allow us to make that
promise, and some new features of `rules_rust` require new Bazel features that
are only available in Bazel rolling releases). `rules_rust` will aim that new
features available to the current Bazel rolling release will be available to the
next Bazel LTS release at latest.

Whenever there is a new Bazel LTS release, all releases of `rules_rust` will maintain
support for the older LTS version for at least 3 months unless Bazel doesn't allow this.

## What host platforms are supported?

Platforms subject to backwards compatibility policy are
`x86_64-unknown-linux-gnu` and `x86_64-apple-darwin` (platforms supported by `rules_rust`).
Process for moving a best effort platform to a supported platform is consensus-based.

## What are the stable APIs of `rules_rust`?

`//rust:defs.bzl` is subject to the backwards compatibility policy. That means
that anything directly accessible from this file is considered stable.

`//rust/private/…` is not subject to the backwards compatibility policy. Content
of this package is an implementation detail.

`//cargo/...` is subject to the backwards compatibility policy.

`//util`, `//tools`, `//test`, `//examples`, `//extensions`, `//ffi`, `//nix` and any packages
not mentioned by this document are by default not subject to the backwards compatibility
policy.

Experimental build settings are not subject to the backward compatibility
policy. They should be added to `//rust:experimental.bzl`.

Incompatible build settings are subject to the backward compatibility policy,
meaning the behavior of the flag cannot change in a backwards incompatible way.
They should be added to `//rust:incompatible.bzl`.

Bug fixes are not a breaking change by default. We'll use Common Sense (and we
will pull in more maintainers and the community to discuss) if we see a certain
bug fix is controversial. Incompatible changes to
`//cargo:defs.bzl` that make `cargo_build_script` more accurately
follow cargo's behavior are considered bug fixes.

## How to make a backwards incompatible change?

1. Create a GitHub issue (example:
[Split rust_library into smaller rules#591](https://github.com/bazelbuild/rules_rust/issues/591)).
2. Describe the change, motivation for the change, provide migration
instructions/tooling.
3. Add a build setting into `//rust:incompatible.bzl` that removes the old
behavior (whenever possible) or changes the current behavior (when just
removing the old behavior is not possible). Ideally, users should not need to
manually flip incompatible flags.
4. Mention the link to the GitHub issue in error messages. Do not add a
deprecation warning (warnings make the deprecation visible to every user
building a project, not only to the maintainers of the project or the rules).
5. Mention the issue in the `CHANGELOG` file.
6. Give the community 3 months from the first release mentioning the issue until
the flag flip to migrate.
