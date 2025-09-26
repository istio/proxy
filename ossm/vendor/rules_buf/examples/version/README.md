# Version

Set the version in the [workspace file](WORKSPACE) and run `bazel run //:print`. It should print the version set in the workspace.

It also demonstrates using the buf cli by depending on the toolchain [here](version.bzl).
