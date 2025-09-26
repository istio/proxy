load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("//private/rules:artifact.bzl", "artifact")

java_library(
    name = "coursier",
    srcs = glob(["*.java"]),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd:__pkg__",
        "//tests/com/github/bazelbuild/rules_jvm_external:__subpackages__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/lockfile",
        artifact(
            "com.google.code.gson:gson",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_binary(
    name = "LockFileConverter",
    main_class = "com.github.bazelbuild.rules_jvm_external.coursier.LockFileConverter",
    visibility = ["//visibility:public"],
    runtime_deps = [
        ":coursier",
    ],
)
