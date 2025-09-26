load("@rules_java//java:defs.bzl", "java_binary", "java_library")

java_library(
    name = "javadoc_lib",
    srcs = ["JavadocJarMaker.java"],
    visibility = [
        "//tests/com/github/bazelbuild/rules_jvm_external/javadoc:__pkg__",
    ],
    deps = [
        ":create_jar_lib",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)

java_binary(
    name = "javadoc",
    main_class = "com.github.bazelbuild.rules_jvm_external.javadoc.JavadocJarMaker",
    visibility = [
        "//visibility:public",
    ],
    runtime_deps = [
        ":javadoc_lib",
    ],
)

java_library(
    name = "create_jar_lib",
    srcs = ["CreateJar.java"],
    visibility = [
        "//tests/com/github/bazelbuild/rules_jvm_external/javadoc:__pkg__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)
