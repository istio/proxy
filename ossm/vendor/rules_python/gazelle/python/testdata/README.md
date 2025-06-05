# Gazelle Python extension test cases

Each directory is a test case that contains `BUILD.in` and `BUILD.out` files for
assertion. `BUILD.in` is used as how the build file looks before running
Gazelle, and `BUILD.out` how the build file should look like after running
Gazelle.

Each test case is a Bazel workspace and Gazelle will run with its working
directory set to the root of this workspace, though, the test runner will find
`test.yaml` files and use them to determine the directory Gazelle should use for
each inner Python project. The `test.yaml` file is a manifest for the test -
check for the existing ones for examples.
