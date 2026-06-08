# For Developers

After cloning the repo, run `pre-commit install` so that your commits are automatically formatted for you. Otherwise you'll get yelled at later by our CI.

> See https://pre-commit.com/#installation if you don't have `pre-commit` installed.

It's a lot of work for us to review and accept Pull Requests. Sometimes things can't merge simply because it's too hard to determine if you change is breaking something else, or because the system you're touching is already being refactored by someone else. Before working on new features, we strongly encourage you to review the project's scope described in the `README.md` file. For large changes, consider writing a design document using [this template](https://goo.gl/YCQttR).

## In-repo tests

A number of tests in this repo are designed to function with local repository
paths, meaning they can be run directly, and are faster to execute. The `npm
test` command is a shortcut to exclude integration tests, eg

    bazel test //...

## Integration tests

In order to test that the rules work outside of this repo, this repo has full bazel WORKSPACEs for e2e tests
to set up an environment with the package paths matching what end users will see.
The end-to-end tests in e2e are built this way.

## Debugging

See [this page](./docs/debugging.md).

## Patching

For small changes, you may find it easier to [patch the standard
rules](./docs/changing-rules.md) instead of building your own release products.

## Releasing

1. Determine the next release version, following semver (could automate in the future from changelog)
2. Tag the repo and push it (or create a tag in GH UI)
3. Watch the automation run on GitHub actions
