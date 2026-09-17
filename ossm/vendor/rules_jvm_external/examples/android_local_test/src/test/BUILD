android_local_test(
    name = "main_activity_test",
    srcs = ["java/com/example/bazel/MainActivityTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.example.bazel.MainActivityTest",
    deps = [
        "//src/main:my_lib",
        "@maven//:androidx_test_ext_junit",
        "@maven//:org_assertj_assertj_core",
        "@maven//:org_robolectric_robolectric",
        "@maven//:org_robolectric_shadows_framework",
        "@robolectric//bazel:android-all",
    ],
)
