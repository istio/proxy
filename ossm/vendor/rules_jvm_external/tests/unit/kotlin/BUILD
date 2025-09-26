load("@rules_kotlin//kotlin:jvm.bzl", "kt_jvm_test")

kt_jvm_test(
    name = "inline_function_test",
    srcs = ["com/example/bazel/InlineFunctionTest.kt"],
    test_class = "com.example.bazel.InlineFunctionTest",
    deps = [
        "@regression_testing_coursier//:junit_junit",
        "@regression_testing_coursier//:org_jetbrains_kotlin_kotlin_test",
    ],
)
