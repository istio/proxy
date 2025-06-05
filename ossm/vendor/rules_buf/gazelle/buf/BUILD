load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@bazel_gazelle//:def.bzl", "gazelle", "gazelle_binary")

# gazelle:exclude testdata/*

go_library(
    name = "buf",
    srcs = [
        "buf.go",
        "buf_lang.go",
        "buf_lock.go",
        "buf_work.go",
        "config.go",
        "cross_resolve.go",
        "generate.go",
        "kinds.go",
        "resolve.go",
        "update.go",
    ],
    importpath = "github.com/bufbuild/rules_buf/gazelle/buf",
    visibility = ["//visibility:public"],
    deps = [
        "@bazel_gazelle//config:go_default_library",
        "@bazel_gazelle//label:go_default_library",
        "@bazel_gazelle//language:go_default_library",
        "@bazel_gazelle//language/proto:go_default_library",
        "@bazel_gazelle//repo:go_default_library",
        "@bazel_gazelle//resolve:go_default_library",
        "@bazel_gazelle//rule:go_default_library",
        "@com_github_bazelbuild_buildtools//build:go_default_library",
        "@in_gopkg_yaml_v3//:yaml_v3",
    ],
)

bzl_library(
    name = "repositories",
    srcs = ["repositories.bzl"],
    visibility = ["//visibility:public"],
    deps = ["@bazel_gazelle//:deps"],
)

# This gazelle binary is used exclusively for testing the gazelle language
# extension.
gazelle_binary(
    name = "gazelle-buf",
    languages = [
        "@bazel_gazelle//language/proto:go_default_library",
        ":buf",
    ],
    visibility = [
        # Also make the binary available in the root of the repo for use, but
        # not externally.
        "//:__pkg__",
    ],
)

gazelle(
    name = "gazelle",
    gazelle = ":gazelle-buf",
)

go_test(
    name = "buf_test",
    srcs = ["buf_test.go"],
    data = [":gazelle-buf"] + glob(["testdata/**"]),
    deps = [
        "@bazel_gazelle//testtools:go_default_library",
        "@com_github_stretchr_testify//require",
        "@io_bazel_rules_go//go/tools/bazel:go_default_library",
    ],
)
