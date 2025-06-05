load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")

swift_library(
    name = "NIOTransportServices",
    srcs = glob([
        "Sources/NIOTransportServices/**/*.swift",
    ]),
    module_name = "NIOTransportServices",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_apple_swift_atomics//:Atomics",
        "@com_github_apple_swift_nio//:NIO",
        "@com_github_apple_swift_nio//:NIOCore",
        "@com_github_apple_swift_nio//:NIOFoundationCompat",
        "@com_github_apple_swift_nio//:NIOTLS",
    ],
)
