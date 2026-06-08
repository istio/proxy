load("@bazel_skylib//rules:native_binary.bzl", "native_binary")
load("@bazel_skylib//rules/directory:directory.bzl", "directory")
load("@bazel_skylib//rules/directory:subdirectory.bzl", "subdirectory")

package(default_visibility = ["//tests/rule_based_toolchain:__subpackages__"])

directory(
    name = "directory",
    srcs = glob(
        ["**"],
        exclude = ["BUILD"],
    ),
)

subdirectory(
    name = "subdirectory_1",
    parent = ":directory",
    path = "subdir1",
)

subdirectory(
    name = "subdirectory_2",
    parent = ":directory",
    path = "subdir2",
)

subdirectory(
    name = "subdirectory_3",
    parent = ":directory",
    path = "subdir3",
)

exports_files(
    glob(
        ["*"],
        exclude = ["BUILD"],
    ),
)

native_binary(
    name = "bin_wrapper",
    src = "bin_wrapper.sh",
    out = "bin_wrapper",
    data = [":bin"],
)

filegroup(
    name = "multiple",
    srcs = [
        "multiple1",
        "multiple2",
    ],
)

# Analysis_test is unable to depend on source files directly, but it can depend
# on a filegroup containing a single file.
filegroup(
    name = "bin_filegroup",
    srcs = ["bin"],
)

# Analysis_test is unable to depend on source files directly, but it can depend
# on a filegroup containing a single file.
filegroup(
    name = "bin_wrapper_filegroup",
    srcs = ["bin_wrapper.sh"],
)
