package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_binary",
)

load("@io_opencensus_cpp//opencensus:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

envoy_cc_library(
    name = "stackdriver_plugin",
    repository = "@envoy",
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver"
    ],
    hdrs = [
        "stackdriver.h",
    ],
    srcs = [
        "stackdriver_plugin_factory.cc",
        "stackdriver.cc",
    ],
    deps = [
        "@envoy//source/extensions/common/wasm/null:null_lib",
        "@io_opencensus_cpp//opencensus/exporters/stats/stackdriver:stackdriver_exporter",
    ],
)
