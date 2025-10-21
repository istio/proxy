licenses(["notice"])  # Apache 2

cc_library(
    name = "openssl",
    srcs = [
        "libssl.so.3",
        "libcrypto.so.3",
    ],
    visibility = ["//visibility:public"],
    linkstatic=False,
)

# Take the shared libs from the system location and copy them into the expected
# structure that will get copied into the runfiles directory when running tests
# executables, thus enabling the compatibility layer to load them.
genrule(
    name = "create_openssl_lib_structure",
    srcs = [
        "libssl.so.3",
        "libcrypto.so.3",
    ],
    outs = [
        "openssl/lib/libssl.so.3",
        "openssl/lib/libcrypto.so.3",
    ],
    cmd = "mkdir -p $$(dirname $(location openssl/lib/libssl.so.3)) && " +
          "cp $(location libssl.so.3) $(location openssl/lib/libssl.so.3) && " +
          "cp $(location libcrypto.so.3) $(location openssl/lib/libcrypto.so.3)",
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
