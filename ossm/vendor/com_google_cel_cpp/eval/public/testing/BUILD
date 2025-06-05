package(
    default_testonly = True,
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "matchers",
    srcs = ["matchers.cc"],
    hdrs = ["matchers.h"],
    deps = [
        "//eval/public:cel_value",
        "//eval/public:set_util",
        "//eval/public:unknown_set",
        "//internal:casts",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "matchers_test",
    srcs = ["matchers_test.cc"],
    deps = [
        ":matchers",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)
