workspace(name = "envoy_examples")

load("//bazel:env.bzl", "envoy_examples_env")
envoy_examples_env()

load("//bazel:archives.bzl", "load_envoy_examples_archives")
load_envoy_examples_archives()

load("//bazel:deps.bzl", "resolve_envoy_examples_dependencies")
resolve_envoy_examples_dependencies()

load("//bazel:toolchains.bzl", "load_envoy_examples_toolchains")
load_envoy_examples_toolchains()

load("//bazel:packages.bzl", "load_envoy_examples_packages")
load_envoy_examples_packages()
