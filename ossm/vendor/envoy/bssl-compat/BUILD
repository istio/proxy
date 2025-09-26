load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
load(":filter_out_shared_libs.bzl", "filter_out_shared_libs")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

licenses(["notice"])  # Apache 2

cmake(
    name = "bssl-compat-build",
    lib_source = ":srcs",
    out_shared_libs = [],
    out_static_libs = ["libbssl-compat.a"],
    visibility = ["//visibility:private"],
    generate_crosstool_file = False,
    out_binaries = ["utests-bssl-compat"],
    build_args = [ "-j" ],
    deps = ["@openssl//:openssl"],
    cache_entries = {"OPENSSL_ROOT_DIR": "$$EXT_BUILD_DEPS/openssl"},
    # Add the OpenSSL shared libraries as a *data* dependency, so they get
    # propagated to dependant targets, and made available in their runfiles
    # directory when they run, so libbssl-compat.a can dlopen() them.
    data = ["@openssl//:libs"],
)

filegroup(
    name = "utests-bssl-compat",
    srcs = [":bssl-compat-build"],
    output_group = "utests-bssl-compat",
    visibility = ["//visibility:public"],
)

# This target is identical to :bssl-compat-build but with the OpenSSL shared
# libraries dependency filtered out, to stop dependants linking against them.
filter_out_shared_libs(
    name = "bssl-compat",
    dep = ":bssl-compat-build",
    visibility = ["//visibility:public"],
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
