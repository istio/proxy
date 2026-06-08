load("@build_bazel_apple_support//rules:universal_binary.bzl", "universal_binary")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

licenses(["notice"])

cc_library(
    name = "compile_with_worker",
    srcs = [
        "compile_with_worker.cc",
        "work_processor.cc",
        "work_processor.h",
    ],
    hdrs = ["compile_with_worker.h"],
    copts = select({
        "//tools:clang-cl": [
            "-Xclang",
            "-fno-split-cold-code",
            "/std:c++17",
        ],
        "//tools:msvc": [
            "/std:c++17",
        ],
        "//conditions:default": [
            "-std=c++17",
        ],
    }),
    deps = [
        ":swift_runner",
        ":worker_protocol",
        "//tools/common:temp_file",
    ],
)

cc_library(
    name = "compile_without_worker",
    srcs = ["compile_without_worker.cc"],
    hdrs = ["compile_without_worker.h"],
    copts = select({
        "//tools:clang-cl": [
            "-Xclang",
            "-fno-split-cold-code",
            "/std:c++17",
        ],
        "//tools:msvc": [
            "/std:c++17",
        ],
        "//conditions:default": [
            "-std=c++17",
        ],
    }),
    deps = [
        ":swift_runner",
    ],
)

cc_library(
    name = "swift_runner",
    srcs = [
        "output_file_map.cc",
        "output_file_map.h",
        "swift_runner.cc",
    ],
    hdrs = ["swift_runner.h"],
    copts = select({
        "//tools:clang-cl": [
            "-Xclang",
            "-fno-split-cold-code",
            "/std:c++17",
        ],
        "//tools:msvc": [
            "/std:c++17",
        ],
        "//conditions:default": [
            "-std=c++17",
        ],
    }),
    data = select({
        "@build_bazel_apple_support//configs:apple": [
            "@build_bazel_rules_swift_index_import//:index_import",
        ],
        "//conditions:default": [],
    }),
    deps = [
        "//tools/common:bazel_substitutions",
        "//tools/common:process",
        "//tools/common:temp_file",
        "@com_github_nlohmann_json//:json",
    ],
)

cc_library(
    name = "worker_protocol",
    srcs = ["worker_protocol.cc"],
    hdrs = ["worker_protocol.h"],
    copts = select({
        "//tools:clang-cl": [
            "-Xclang",
            "-fno-split-cold-code",
            "/std:c++17",
        ],
        "//tools:msvc": [
            "/std:c++17",
        ],
        "//conditions:default": [
            "-std=c++17",
        ],
    }),
    deps = [
        "@com_github_nlohmann_json//:json",
    ],
)

cc_binary(
    name = "worker",
    srcs = ["worker_main.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":compile_with_worker",
        ":compile_without_worker",
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

universal_binary(
    name = "universal_worker",
    binary = ":worker",
    visibility = ["//visibility:public"],
)

alias(
    name = "worker_wrapper",
    actual = select({
        "//swift:universal_tools_config": ":universal_worker",
        "//conditions:default": ":worker",
    }),
    visibility = ["//visibility:public"],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//tools:__pkg__",
    ],
)
