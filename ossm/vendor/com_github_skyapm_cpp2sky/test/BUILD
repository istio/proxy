load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "mocks",
    hdrs = [
        "mocks.h",
    ],
    deps = [
        "//cpp2sky/internal:async_client_interface",
        "//cpp2sky/internal:random_generator_interface",
        "@com_google_googletest//:gtest_main",
        "@skywalking_data_collect_protocol//language-agent:tracing_protocol_cc_proto",
    ],
)

cc_test(
    name = "propagation_test",
    srcs = [
        "propagation_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//source:cpp2sky_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracing_context_test",
    srcs = [
        "tracing_context_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mocks",
        "//source:cpp2sky_lib",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "grpc_async_client_test",
    srcs = [
        "grpc_async_client_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mocks",
        "//source:cpp2sky_lib",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "buffer_test",
    srcs = [
        "buffer_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mocks",
        "//source:cpp2sky_lib",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_test",
    srcs = [
        "tracer_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mocks",
        "//source:cpp2sky_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "matcher_test",
    srcs = [
        "matcher_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mocks",
        "//source:cpp2sky_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
