load("@rules_java//java:java_binary.bzl", "java_binary")

package(default_visibility = ["//visibility:public"])

java_binary(
    name = "stardoc_renderer",
    main_class = "com/google/devtools/build/stardoc/renderer/RendererMain",
    runtime_deps = ["@stardoc-prebuilt//jar"],
)
