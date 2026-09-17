android_library(
    name = "my_lib",
    exports = [
        "@junit_junit//jar",  # alias to @maven//:junit_junit
        "@maven//:android_arch_lifecycle_common",
        "@maven//:android_arch_lifecycle_viewmodel",
        "@maven//:androidx_test_espresso_espresso_web",
        "@maven//:com_android_support_design",
    ],
)

android_binary(
    name = "my_app",
    custom_package = "com.example.bazel",
    manifest = "AndroidManifest.xml",
    deps = [":my_lib"],
)
