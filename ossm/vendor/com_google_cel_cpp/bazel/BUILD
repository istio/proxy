load("@rules_java//java:defs.bzl", "java_binary")

java_binary(
    name = "antlr4_tool",
    main_class = "org.antlr.v4.Tool",
    runtime_deps = ["@antlr4_jar//jar"],
)

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "cel_cc_embed",
    srcs = ["cel_cc_embed.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
