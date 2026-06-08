"""Provides a simple way to test your rules as an external workspace.
Add a basic smoke-test target below.
"""

load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@yq.bzl", "yq")

yq(
    name = "no_srcs",
    srcs = [],
    expression = ".name = \"Alice\"",
)

write_file(
    name = "expected",
    out = "expected.yaml",
    content = [
        "name: Alice",
        "",
    ],
)

diff_test(
    name = "smoke_test",
    file1 = "no_srcs.yaml",
    file2 = "expected.yaml",
)
