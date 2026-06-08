load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_java//java:java_library.bzl", "java_library")

java_library(
    name = "javadoc_lib",
    srcs = ["JavadocJarMaker.java"],
    visibility = [
        "//tests/com/github/bazelbuild/rules_jvm_external/javadoc:__pkg__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:create_jar_lib",
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
