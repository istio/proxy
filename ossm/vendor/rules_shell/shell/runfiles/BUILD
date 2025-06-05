load("//shell:sh_library.bzl", "sh_library")
load("//shell/private:root_symlinks.bzl", "ROOT_SYMLINKS_SUPPORTED", "root_symlinks")

alias(
    name = "runfiles",
    actual = ":runfiles_impl" if ROOT_SYMLINKS_SUPPORTED else "@bazel_tools//tools/bash/runfiles",
    visibility = ["//visibility:public"],
)

sh_library(
    name = "runfiles_impl",
    data = [":runfiles_at_legacy_location"],
    tags = ["manual"],
)

root_symlinks(
    name = "runfiles_at_legacy_location",
    root_symlinks = {
        "runfiles.bash": "bazel_tools/tools/bash/runfiles/runfiles.bash",
    },
    tags = ["manual"],
)
