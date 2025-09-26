# Use private rule implementations directly if all required features are available to provide test
# coverage for Bazel 8.
load("@bazel_features//:features.bzl", "bazel_features")
load("@rules_shell//shell/private:sh_binary.bzl", "sh_binary")  # buildifier: disable=bzl-visibility
load("@rules_shell//shell/private:sh_library.bzl", "sh_library")  # buildifier: disable=bzl-visibility
load("@rules_shell//shell/private:sh_test.bzl", "sh_test")  # buildifier: disable=bzl-visibility

sh_library(
    name = "lib",
    srcs = ["lib.sh"],
    data = ["greeting.txt"],
)

bazel_features.rules._has_launcher_maker_toolchain and sh_binary(
    name = "bin",
    srcs = ["bin.sh"],
    use_bash_launcher = True,
    deps = [":lib"],
)  # buildifier: disable=no-effect

bazel_features.rules._has_launcher_maker_toolchain and sh_test(
    name = "test",
    srcs = ["test.sh"],
    data = [":bin"],
    use_bash_launcher = True,
)  # buildifier: disable=no-effect
