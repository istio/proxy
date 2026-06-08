load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_binary(
    name = "make_xcarchive",
    srcs = ["make_xcarchive.py"],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = ["//tools/wrapper_common:execute"],
)
