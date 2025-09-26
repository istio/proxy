# Support Policy

The Bazel community maintains this repository. Neither Google nor the Bazel team
provides support for the code. However, this repository is part of the test
suite used to vet new Bazel releases. See the <project:#contributing>
page for information on our development workflow.

## Supported rules_python Versions

In general, only the latest version is supported. Backporting changes is
done on a best-effort basis based on severity, risk of regressions, and
the willingness of volunteers.

If you want or need particular functionality backported, then the best way
is to open a PR to demonstrate the feasibility of the backport.

### Backports and Patch Releases

Backports and patch releases are provided on a best-effort basis. Only fixes are
backported. Features are not backported.

Backports can be done to older releases, but only if newer releases also have
the fix backported. For example, if the current release is 1.5, in order to
patch 1.4, version 1.5 must be patched first.

Backports can be requested by [creating an issue with the patch release
template][patch-release-issue] or by sending a pull request performing the backport.
See the dev guide for [how to create a backport PR][backport-pr].

[patch-release-issue]: https://github.com/bazelbuild/rules_python/issues/new?template=patch_release_request.md
[backport-pr]: devguide.html#creating-backport-prs

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

## Supported Python versions

As a general rule, we test all released non-EOL Python versions. Different
interpreter versions may work but are not guaranteed. We are interested in
staying compatible with upcoming unreleased versions, so if you see that things
stop working, please create tickets or, more preferably, pull requests.

## Supported Platforms

We only support the platforms that our continuous integration jobs run on, which
are Linux, Mac, and Windows.

In order to better describe different support levels, the following acts as a rough
guideline for different platform tiers:
* Tier 0 - The platforms that our CI runs on: `linux_x86_64`, `osx_x86_64`, `RBE linux_x86_64`.
* Tier 1 - The platforms that are similar enough to what the CI runs on: `linux_aarch64`, `osx_arm64`.
  What is more, `windows_x86_64` is in this list, as we run tests in CI, but
  developing for Windows is more challenging, and features may come later to
  this platform.
* Tier 2 - The rest of the platforms that may have a varying level of support, e.g.,
  `linux_s390x`, `linux_ppc64le`, `windows_arm64`.

:::{note}
Code to support Tier 2 platforms is allowed, but regressions will be fixed on a
best-effort basis, so feel free to contribute by creating PRs.

If you would like to provide/sponsor CI setup for a platform that is not Tier 0,
please create a ticket or contact the maintainers on Slack.
:::

## Compatibility Policy

We generally follow the [Bazel Rule
Compatibility](https://bazel.build/release/rule-compatibility) guidelines, which
provides a path from an arbitrary release to the latest release in an
incremental fashion.

Breaking changes are allowed, but follow a process to introduce them over
a series of releases to so users can still incrementally upgrade. See the
[Breaking Changes](#breaking-changes) doc for the process.


## Experimental Features

An experimental feature is functionality that may not be ready for general
use and may change quickly and/or significantly. Such features are denoted in
their name or API docs as "experimental". They may have breaking changes made at
any time.

If you like or use an experimental feature, then file issues to request it be
taken out of experimental. Often times these features are experimental because
we need feedback or experience to verify they are working, useful, and worth the
effort of supporting.
