# Bazel-in-Bazel integration tests

The tests in this directory are Bazel-in-Bazel integration tests. These are
necessary because our CI has a limit of 80 jobs, and our test matrix uses most
of those for more important end-to-end tests of user-facing examples.

The tests in here are more for testing internal aspects of the rules that aren't
easily tested as tests run by Bazel itself (basically anything that happens
prior to the analysis phase).

## Adding a new directory

When adding a new diretory, a couple files need to be updated to tell the outer
Bazel to ignore the nested workspace.

* Add the directory to the `--deleted_packages` flag. Run `pre-commit` and it
  will do this for you. This also allows the integration test to see the
  nested workspace files correctly.
* Update `.bazelignore` and add `tests/integration/<directory>/bazel-<name>`.
  This prevents Bazel from following infinite symlinks and freezing.
* Add a `rules_python_integration_test` target to the BUILD file.
