
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
bzl_library(
  name = "proxy_bzl",
  srcs = ["proxy.bzl"],
  deps = ["@rules_cc//cc/private/rules_impl:native_bzl"],
  visibility = ["@rules_cc//cc:__subpackages__"],
)
            