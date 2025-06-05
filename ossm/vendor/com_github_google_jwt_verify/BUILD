licenses(["notice"])

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "jwt_verify_lib",
    srcs = [
        "src/check_audience.cc",
        "src/jwks.cc",
        "src/jwt.cc",
        "src/status.cc",
        "src/struct_utils.cc",
        "src/verify.cc",
    ],
    hdrs = [
        "jwt_verify_lib/check_audience.h",
        "jwt_verify_lib/jwks.h",
        "jwt_verify_lib/jwt.h",
        "jwt_verify_lib/status.h",
        "jwt_verify_lib/struct_utils.h",
        "jwt_verify_lib/verify.h",
    ],
    deps = [
        "//external:abseil_flat_hash_set",
        "//external:abseil_strings",
        "//external:abseil_time",
        "//external:protobuf",
        "//external:ssl",
    ],
)

cc_library(
    name = "simple_lru_cache_lib",
    hdrs = [
        "simple_lru_cache/simple_lru_cache.h",
        "simple_lru_cache/simple_lru_cache_inl.h",
    ],
    deps = [
        "//external:abseil_flat_hash_map",
    ],
)

cc_test(
    name = "check_audience_test",
    timeout = "short",
    srcs = [
        "test/check_audience_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "jwt_test",
    timeout = "short",
    srcs = [
        "test/jwt_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "jwks_test",
    timeout = "short",
    srcs = [
        "test/jwks_test.cc",
        "test/test_common.h",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "simple_lru_cache_test",
    timeout = "short",
    srcs = [
        "test/simple_lru_cache_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":simple_lru_cache_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_x509_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_x509_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_audiences_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_audiences_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "jwt_time_test",
    timeout = "short",
    srcs = [
        "test/jwt_time_test.cc",
        "test/test_common.h",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_jwk_rsa_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_jwk_rsa_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_jwk_rsa_pss_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_jwk_rsa_pss_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_jwk_ec_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_jwk_ec_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_jwk_hmac_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_jwk_hmac_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_jwk_okp_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_jwk_okp_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_pem_rsa_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_pem_rsa_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_pem_ec_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_pem_ec_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "verify_pem_okp_test",
    timeout = "short",
    srcs = [
        "test/test_common.h",
        "test/verify_pem_okp_test.cc",
    ],
    linkopts = [
        "-lm",
        "-lpthread",
    ],
    linkstatic = 1,
    deps = [
        ":jwt_verify_lib",
        "//external:googletest_main",
    ],
)
