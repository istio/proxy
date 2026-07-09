load("@rules_cc//cc:cc_library.bzl", "cc_library")

licenses(["notice"])  # Apache 2

# Export all OpenSSL headers - the genrule will extract directory from the first one
# The BoringSSL compat layer's genrule uses $(location @@openssl//:include)
# filegroup(
#     name = "include",
#     srcs = glob(["include/openssl/*.h"]),
#     visibility = ["//visibility:public"],
# )

exports_files(
    ["include"],
    visibility = ["//visibility:public"],
)

# All OpenSSL headers for cc_library targets
# filegroup(
#     name = "include_headers",
#     srcs = glob(["include/openssl/**/*.h"]),
#     visibility = ["//visibility:public"],
# )

cc_library(
    name = "crypto",
    srcs = ["lib64/libcrypto.so.3"],
    hdrs = glob(["include/openssl/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    linkstatic=False,
)

cc_library(
    name = "ssl",
    srcs = ["lib64/libssl.so.3"],
    hdrs = glob(["include/openssl/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    linkstatic=False,
    deps = [":crypto"],
)

cc_library(
    name = "openssl",
    visibility = ["//visibility:public"],
    deps = [":ssl", ":crypto"],
)

# Take the shared libs from the system location and copy them into the expected
# structure that will get copied into the runfiles directory when running tests
# executables, thus enabling the compatibility layer to load them.
genrule(
    name = "create_openssl_lib_structure",
    srcs = [
        "lib64/libssl.so.3",
        "lib64/libcrypto.so.3",
    ],
    outs = [
        "openssl/lib/libssl.so.3",
        "openssl/lib/libcrypto.so.3",
    ],
    cmd = "mkdir -p $$(dirname $(location openssl/lib/libssl.so.3)) && " +
          "cp $(location lib64/libssl.so.3) $(location openssl/lib/libssl.so.3) && " +
          "cp $(location lib64/libcrypto.so.3) $(location openssl/lib/libcrypto.so.3)",
    visibility = ["//visibility:private"],
)

# This is the target that @envoy//bssl-compat:bssl-compat depends on as a *data*
# dependency, so that the OpenSSL shared libraries are made available in the
# runfiles directory when running tests executables.
filegroup(
    name = "libs",
    srcs = [":create_openssl_lib_structure"],
    visibility = ["//visibility:public"],
)
