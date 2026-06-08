
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
bzl_library(
  name = "proxy_bzl",
  srcs = ["proxy.bzl"],
  visibility = ["@rules_cc//cc:__subpackages__"],
)
bzl_library(
  name = "symbols_bzl",
  srcs = ["symbols.bzl"],
  deps = [
      "@rules_cc//cc/private/rules_impl:native_cc_common_bzl",
      "@rules_cc//cc/private/rules_impl:native_providers_bzl",
  ],
  visibility = ["@rules_cc//cc:__subpackages__"],
)
            