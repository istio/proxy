"""Provides a simple way to test your rules as an external workspace.
Add a basic smoke-test target below.
"""

load("@aspect_bazel_lib//lib:testing.bzl", "assert_json_matches")
load("@jq.bzl", "jq")

jq(
    name = "no_srcs",
    srcs = [],
    filter = ".name = \"Alice\"",
)

assert_json_matches(
    name = "smoke_test",
    file1 = "no_srcs.json",
    file2 = "no_srcs.json",
    filter1 = ".name",
    filter2 = "\"Alice\"",
)
