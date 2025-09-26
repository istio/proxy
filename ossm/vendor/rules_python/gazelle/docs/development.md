# Development

Gazelle extensions are written in Go.

See the [Gazelle documentation][gazelle-extend] for more information on
extending Gazelle.

[gazelle-extend]: https://github.com/bazel-contrib/bazel-gazelle/blob/master/extend.md


## Dependencies

If you add new Go dependencies to the plugin source code, you need to "tidy"
the go.mod file.  After changing that file, run `go mod tidy` or
`bazel run @go_sdk//:bin/go -- mod tidy` to update the `go.mod` and `go.sum`
files. Then run `bazel run //:gazelle_update_repos` to have gazelle add the
new dependencies to the `deps.bzl` file. The `deps.bzl` file is used as
defined in our `/WORKSPACE` to include the external repos Bazel loads Go
dependencies from.

Then after editing Go code, run `bazel run //:gazelle` to generate/update
the rules in the `BUILD.bazel` files in our repo.


## Tests

:::{seealso}
{gh-path}`gazelle/python/testdata/README.md`
:::

To run tests, {command}`cd` into the {gh-path}`gazelle` directory and run
`bazel test //...`.

Test cases are found at {gh-path}`gazelle/python/testdata`. To make a new
test case, create a directory in that folder with the following files:

+ `README.md` with a short blurb describing the test case(s).
+ `test.yaml`, either empty (with just the docstart `---` line) or with
  the expected `stderr` and exit codes of the test case.
+ and empty `WORKSPACE` file

You will also need `BUILD.in` and `BUILD.out` files somewhere within the test
case directory. These can be in the test case root, in subdirectories, or
both.

+ `BUILD.in` files are populated with the "before" information - typically
  things like Gazelle directives or pre-existing targets. This is how the
  `BUILD.bazel` file looks before running Gazelle.
+ `BUILD.out` files are the expected result after running Gazelle within
  the test case.

:::{tip}
The easiest way to create a new test is to look at one of the existing test
cases.
:::

The source code for running tests is {gh-path}`gazelle/python/python_test.go`.
