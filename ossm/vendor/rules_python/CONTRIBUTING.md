# How to contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Getting started

Before we can work on the code, we need to get a copy of it and setup some
local environment and tools.

First, fork the code to your user and clone your fork. This gives you a private
playground where you can do any edits you'd like. For this guide, we'll use
the [GitHub `gh` tool](https://github.com/cli/cli)
([Linux install](https://github.com/cli/cli/blob/trunk/docs/install_linux.md)).
(More advanced users may prefer the GitHub UI and raw `git` commands).

```shell
gh repo fork bazelbuild/rules_python --clone --remote
```

Next, make sure you have a new enough version of Python installed that supports the
various code formatters and other devtools. For a quick start,
[install pyenv](https://github.com/pyenv/pyenv-installer) and
at least Python 3.9.15:

```shell
curl https://pyenv.run | bash
pyenv install 3.9.15
pyenv shell 3.9.15
```

## Development workflow

It's suggested that you create what is called a "feature/topic branch" in your
fork when you begin working on code you want to eventually send or code review.

```
git checkout main # Start our branch from the latest code
git checkout -b my-feature # Create and switch to our feature branch
git push origin my-feature # Cause the branch to be created in your fork.
```

From here, you then edit code and commit to your local branch. If you want to
save your work to github, you use `git push` to do so:

```
git push origin my-feature
```

Once the code is in your github repo, you can then turn it into a Pull Request
to the actual rules_python project and begin the code review process.


## Running tests

Running tests is particularly easy thanks to Bazel, simply run:

```
bazel test //...
```

And it will run all the tests it can find. The first time you do this, it will
probably take long time because various dependencies will need to be downloaded
and setup. Subsequent runs will be faster, but there are many tests, and some of
them are slow. If you're working on a particular area of code, you can run just
the tests in those directories instead, which can speed up your edit-run cycle.

## Updating tool dependencies

It's suggested to routinely update the tool versions within our repo - some of the
tools are using requirement files compiled by `uv` and others use other means. In order
to have everything self-documented, we have a special target -
`//private:requirements.update`, which uses `rules_multirun` to run in sequence all
of the requirement updating scripts in one go. This can be done once per release as
we prepare for releases.

## Formatting

Starlark files should be formatted by
[buildifier](https://github.com/bazelbuild/buildtools/blob/master/buildifier/README.md).
Otherwise the Buildkite CI will fail with formatting/linting violations.
We suggest using a pre-commit hook to automate this.
First [install pre-commit](https://pre-commit.com/#installation),
then run

```shell
pre-commit install
```

### Running buildifer manually

You can also run buildifier manually. To do this,
[install buildifier](https://github.com/bazelbuild/buildtools/blob/master/buildifier/README.md),
and run the following command:

```shell
$ buildifier --lint=fix --warnings=native-py -warnings=all WORKSPACE
```

Replace the argument "WORKSPACE" with the file that you are linting.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution,
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult [GitHub Help] for more
information on using pull requests.

[GitHub Help]: https://help.github.com/articles/about-pull-requests/

### Commit messages

Commit messages (upon merging) and PR messages should follow the [Conventional
Commits](https://www.conventionalcommits.org/) style:

```
type(scope)!: <summary>

<body>

BREAKING CHANGE: <summary>
```

Where `(scope)` is optional, and `!` is only required if there is a breaking change.
If a breaking change is introduced, then `BREAKING CHANGE:` is required; see
the [Breaking Changes](#breaking-changes) section for how to introduce breaking
changes.

User visible changes, such as features, fixes, or notable refactors, should
be documneted in CHANGELOG.md and their respective API doc. See [Documenting
changes] for how to do so.

Common `type`s:

* `build:` means it affects the building or development workflow.
* `docs:` means only documentation is being added, updated, or fixed.
* `feat:` means a user-visible feature is being added. See [Documenting version
  changes] for how to documenAdd `{versionadded}`
  to appropriate docs.
* `fix:` means a user-visible behavior is being fixed. If the fix is changing
  behavior of a function, add `{versionchanged}` to appropriate docs, as necessary.
* `refactor:` means some sort of code cleanup that doesn't change user-visible
  behavior. Add `{versionchanged}` to appropriate docs, as necessary.
* `revert:` means a prior change is being reverted in some way.
* `test:` means only tests are being added.

For the full details of types, see
[Conventional Commits](https://www.conventionalcommits.org/).

### Documenting changes

Changes are documented in two places: CHANGELOG.md and API docs.

CHANGELOG.md contains a brief, human friendly, description. This text is
intended for easy skimming so that, when people upgrade, they can quickly get a
sense of what's relevant to them.

API documentation are the doc strings for functions, fields, attributes, etc.
When user-visible or notable behavior is added, changed, or removed, the
`{versionadded}`, `{versionchanged}` or `{versionremoved}` directives should be
used to note the change. When specifying the version, use the values
`VERSION_NEXT_FEATURE` or `VERSION_NEXT_PATCH` to indicate what sort of
version increase the change requires.

These directives use Sphinx MyST syntax, e.g.

```
:::{versionadded} VERSION_NEXT_FEATURE
The `allow_new_thing` arg was added.
:::

:::{versionchanged} VERSION_NEXT_PATCH
Large numbers no longer consume exponential memory.
:::

:::{versionremoved} VERSION_NEXT_FEATURE
The `legacy_foo` arg was removed
:::
```

## Generated files

Some checked-in files are generated and need to be updated when a new PR is
merged:

* **requirements lock files**: These are usually generated by a
  `compile_pip_requirements` update target, which is usually in the same directory.
  e.g. `bazel run //docs:requirements.update`

## Core rules

The bulk of this repo is owned and maintained by the Bazel Python community.
However, since the core Python rules (`py_binary` and friends) are still
bundled with Bazel itself, the Bazel team retains ownership of their stubs in
this repository. This will be the case at least until the Python rules are
fully migrated to Starlark code.

Practically, this means that a Bazel team member should approve any PR
concerning the core Python logic. This includes everything under the `python/`
directory except for `pip.bzl` and `requirements.txt`.

Issues should be triaged as follows:

- Anything concerning the way Bazel implements the core Python rules should be
  filed under [bazelbuild/bazel](https://github.com/bazelbuild/bazel), using
  the label `team-Rules-python`.

- If the issue specifically concerns the rules_python stubs, it should be filed
  here in this repository and use the label `core-rules`.

- Anything else, such as feature requests not related to existing core rules
  functionality, should also be filed in this repository but without the
  `core-rules` label.

(breaking-changes)=
## Breaking Changes

Breaking changes are generally permitted, but we follow a 3-step process for
introducing them. The intent behind this process is to balance the difficulty of
version upgrades for users, maintaining multiple code paths, and being able to
introduce modern functionality.

The general process is:

1. In version `N`, introduce the new behavior, but it must be disabled by
   default. Users can opt into the new functionality when they upgrade to
   version `N`, which lets them try it and verify functionality.
2. In version `N+1`, the new behavior can be enabled by default. Users can
   opt out if necessary, but doing so causes a warning to be issued.
3. In version `N+2`, the new behavior is always enabled and cannot be opted out
   of. The API for the control mechanism can be removed in this release.

Note that the `+1` and `+2` releases are just examples; the steps are not
required to happen in immediately subsequent releases.

Once The first major version is released, the process will be:
1. In `N.M.0` we introduce the new behaviour, but it is disabled by a feature flag.
2. In `N.M+1.0` we may choose the behaviour to become the default if it is not too
   disruptive.
3. In `N+1.0.0` we get rid of the old behaviour.

### How to control breaking changes

The details of the control mechanism will depend on the situation. Below is
a summary of some different options.

* Environment variables are best for repository rule behavior. Environment
  variables can be propagated to rules and macros using the generated
  `@rules_python_internal//:config.bzl` file.
* Attributes are applicable to macros and regular rules, especially when the
  behavior is likely to vary on a per-target basis.
* [User defined build settings](https://bazel.build/extending/config#user-defined-build-settings)
  (aka custom build flags) are applicable for rules when the behavior change
  generally wouldn't vary on a per-target basis. They also have the benefit that
  an entire code base can have them easily enabled by a bazel command line flag.
* Allowlists allow a project to centrally control if something is
  enabled/disabled. Under the hood, they are basically a specialized custom
  build flag.

Note that attributes and flags can seamlessly interoperate by having the default
controlled by a flag, and an attribute can override the flag setting. This
allows a project to enable the new behavior by default while they work to fix
problematic cases to prepare for the next upgrade.

### What is considered a breaking change?

Precisely defining what constitutes a breaking change is hard because it's
easy for _someone, somewhere_ to depend on _some_ observable behavior, despite
our best efforts to thoroughly document what is or isn't supported and hiding
any internal details.

In general, something is considered a breaking change when it changes the
direct behavior of a supported public API. Simply being able to observe a
behavior change doesn't necessarily mean it's a breaking change.

Long standing undocumented behavior is a large grey area and really depends on
how load-bearing it has become and what sort of reasonable expectation of
behavior there is.

Here's some examples of what would or wouldn't be considered a breaking change.

Breaking changes:
  * Renaming an function argument for public functions.
  * Enforcing stricter validation than was previously required when there's a
    sensible reason users would run afoul of it.
  * Changing the name of a public rule.

Not breaking changes:
  * Upgrading dependencies
  * Changing internal details, such as renaming an internal file.
  * Changing a rule to a macro.

## FAQ

### Installation errors when during `git commit`

If you did `pre-commit install`, various tools are run when you do `git commit`.
This might show as an error such as:

```
[INFO] Installing environment for https://github.com/psf/black.
[INFO] Once installed this environment will be reused.
[INFO] This may take a few minutes...
An unexpected error has occurred: CalledProcessError: command: ...
```

To fix, you'll need to figure out what command is failing and why. Because these
are tools that run locally, its likely you'll need to fix something with your
environment or the installation of the tools. For Python tools (e.g. black or
isort), you can try using a different Python version in your shell by using
tools such as [pyenv](https://github.com/pyenv/pyenv).
