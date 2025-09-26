load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")
# load cc_proto_library

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "jwt_verify_lib_fuzz_input_proto",
    testonly = 1,
    srcs = ["jwt_verify_lib_fuzz_input.proto"],
)

cc_proto_library(
    name = "jwt_verify_lib_fuzz_input_cc_proto",
    testonly = 1,
    deps = [":jwt_verify_lib_fuzz_input_proto"],
)

cc_fuzz_test(
    name = "jwt_verify_lib_fuzz_test",
    testonly = 1,
    srcs = [
        "jwt_verify_lib_fuzz_test.cc",
    ],
    corpus = glob(["corpus/jwt_verify_lib_fuzz_test/*"]),
    deps = [
        ":jwt_verify_lib_fuzz_input_cc_proto",
        "//:jwt_verify_lib",
        "//external:libprotobuf_mutator",
    ],
)

cc_test(
    name = "corpus_format_test",
    testonly = 1,
    srcs = [
        "corpus_format_test.cc",
    ],
    data = glob(["corpus/jwt_verify_lib_fuzz_test/*"]),
    deps = [
        ":jwt_verify_lib_fuzz_input_cc_proto",
        "//:jwt_verify_lib",
        "//external:abseil_strings",
        "//external:googletest_main",
    ],
)
