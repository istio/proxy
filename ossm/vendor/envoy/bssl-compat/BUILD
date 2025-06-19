load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

licenses(["notice"])  # Apache 2

cmake(
    name = "bssl-compat",
    lib_source = ":srcs",
    out_shared_libs = [],
    out_static_libs = ["libbssl-compat.a"],
    visibility = ["//visibility:public"],
    generate_crosstool_file = False,
    out_binaries = ["utests-bssl-compat"],
    build_args = [ "-j" ]
)

filegroup(
    name = "bssl-compat-utests-bssl-compat",
    srcs = [":bssl-compat"],
    output_group = "utests-bssl-compat",
)

sh_test(
    name = "utests-bssl-compat",
    size = "small",
    srcs = [":bssl-compat-utests-bssl-compat"],
)

alias(
    name = "crypto",
    actual = ":bssl-compat",
    visibility = ["//visibility:public"],
)

alias(
    name = "ssl",
    actual = ":bssl-compat",
    visibility = ["//visibility:public"],
)
