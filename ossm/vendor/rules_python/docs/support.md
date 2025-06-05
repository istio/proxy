# Support Policy

The Bazel community maintains this repository. Neither Google nor the Bazel team
provides support for the code. However, this repository is part of the test
suite used to vet new Bazel releases. See the <project:#contributing>
page for information on our development workflow.

## Supported rules_python Versions

In general, only the latest version is supported. Backporting changes is
done on a best effort basis based on severity, risk of regressions, and
the willingness of volunteers.

If you want or need particular functionality backported, then the best way
is to open a PR to demonstrate the feasibility of the backport.

## Supported Bazel Versions

The supported Bazel versions are:

1. The latest rolling release
2. The active major release.
3. The major release prior to the active release.

For (2) and (3) above, only the latest minor/patch version of the major release
is officially supported. Earlier minor/patch versions are supported on a
best-effort basis only. We increase the minimum minor/patch version as necessary
to fix bugs or introduce functionality relying on features introduced in later
minor/patch versions.

See [Bazel's release support matrix](https://bazel.build/release#support-matrix)
for what versions are the rolling, active, and prior releases.

## Supported Platforms

We only support the platforms that our continuous integration jobs run, which
is Linux, Mac, and Windows. Code to support other platforms is allowed, but
can only be on a best-effort basis.

## Compatibility Policy

We generally follow the [Bazel Rule
Compatibility](https://bazel.build/release/rule-compatibility) guidelines, which
provides a path from an arbitrary release to the latest release in an
incremental fashion.

Breaking changes are allowed, but follow a process to introduce them over
a series of releases to so users can still incrementally upgrade. See the
[Breaking Changes](#breaking-changes) doc for the process.


## Experimental Features

An experimental features is functionality that may not be ready for general
use and may change quickly and/or significantly. Such features are denoted in
their name or API docs as "experimental". They may have breaking changes made at
any time.

If you like or use an experimental feature, then file issues to request it be
taken out of experimental. Often times these features are experimental because
we need feedback or experience to verify they are working, useful, and worth the
effort of supporting.
