load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cpp2sky_lib",
    srcs = [
        "grpc_async_client_impl.cc",
        "tracer_impl.cc",
    ],
    hdrs = [
        "grpc_async_client_impl.h",
        "tracer_impl.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":cpp2sky_data_lib",
        "//cpp2sky:config_cc_proto",
        "//cpp2sky:cpp2sky_data_interface",
        "//cpp2sky:cpp2sky_interface",
        "//cpp2sky/internal:async_client_interface",
        "//cpp2sky/internal:matcher_interface",
        "//source/matchers:suffix_matcher_lib",
        "//source/utils:util_lib",
        "@com_github_gabime_spdlog//:spdlog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@skywalking_data_collect_protocol//language-agent:configuration_discovery_service_cc_grpc",
        "@skywalking_data_collect_protocol//language-agent:configuration_discovery_service_cc_proto",
        "@skywalking_data_collect_protocol//language-agent:tracing_protocol_cc_grpc",
        "@skywalking_data_collect_protocol//language-agent:tracing_protocol_cc_proto",
    ],
)

cc_library(
    name = "cpp2sky_data_lib",
    srcs = [
        "propagation_impl.cc",
        "tracing_context_impl.cc",
    ],
    hdrs = [
        "propagation_impl.h",
        "tracing_context_impl.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cpp2sky:config_cc_proto",
        "//cpp2sky:cpp2sky_data_interface",
        "//source/utils:util_lib",
        "@com_google_absl//absl/memory",
        "@skywalking_data_collect_protocol//language-agent:tracing_protocol_cc_proto",
    ],
)
