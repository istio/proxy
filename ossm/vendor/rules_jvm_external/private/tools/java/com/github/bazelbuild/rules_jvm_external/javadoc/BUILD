load("@rules_java//java:defs.bzl", "java_binary")

java_binary(
    name = "javadoc",
    srcs = glob(["*.java"]),
    main_class = "com.github.bazelbuild.rules_jvm_external.javadoc.JavadocJarMaker",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)
