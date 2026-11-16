# Source-built pigz (the `pigz` module, a dev dependency): the published per-platform packages
# (`//pigz/source:pigz_packages`), and the source toolchain of toolshed's own tests (registered
# as a dev dependency, so not for consumers: see //pigz:README.md).

load("//pigz:defs.bzl", "pigz_toolchain")
load("//pigz/private:pigz_package.bzl", "pigz_package")

pigz_toolchain(
    name = "source_impl",
    pigz = "@pigz",
)

# One per supported OS: the `pigz` module builds on POSIX (pthreads), so elsewhere (eg Windows)
# resolution fails cleanly rather than selecting a toolchain that cannot build.
[
    toolchain(
        name = "source_toolchain_%s" % os,
        target_compatible_with = ["@platforms//os:%s" % os],
        toolchain = ":source_impl",
        toolchain_type = "//pigz:toolchain_type",
        visibility = ["//visibility:public"],
    )
    for os in [
        "linux",
        "macos",
    ]
]

[
    pigz_package(
        name = "pigz_%s" % platform.replace("-", "_"),
        licenses = {
            "//pigz/private:licenses/pigz": "pigz/LICENSE",
            "//pigz/private:licenses/zlib-ng": "zlib-ng/LICENSE.md",
            "//pigz/private:licenses/zopfli": "zopfli/COPYING",
        },
        pigz = "@pigz",
        platform = platform,
        stripper = "//compile:llvm_minimal_host_llvm_strip",
        visibility = ["//pigz/test:__pkg__"],
    )
    for platform in [
        "linux-aarch64",
        "linux-x86_64",
    ]
]

filegroup(
    name = "pigz_packages",
    srcs = [
        ":pigz_linux_aarch64",
        ":pigz_linux_x86_64",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)
