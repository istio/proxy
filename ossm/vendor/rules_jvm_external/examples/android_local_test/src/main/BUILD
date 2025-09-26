load("@rules_jvm_external//:defs.bzl", "artifact")

android_library(
    name = "my_lib",
    srcs = glob(["java/com/example/bazel/*.java"]),
    custom_package = "com.example.bazel",
    manifest = "java/AndroidManifest.xml",
    resource_files = glob(["res/**"]),
    visibility = ["//src/test:__subpackages__"],
    deps = [
        artifact("androidx.appcompat:appcompat"),
        artifact("androidx.annotation:annotation"),
    ],
)
