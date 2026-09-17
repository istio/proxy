load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_binary(
    name = "AddJarManifestEntry",
    srcs = ["AddJarManifestEntry.java"],
    # Disable perfdata to avoid warning on Linux hosts (https://github.com/bazelbuild/bazel/issues/3236)
    # [warning][perf,memops] Cannot use file /tmp/hsperfdata_user/2 because it is locked by another process (errno = 11)
    jvm_flags = ["-XX:-UsePerfData"],
    main_class = "com.github.bazelbuild.rules_jvm_external.jar.AddJarManifestEntry",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)

java_library(
    name = "IndexJar-lib",
    srcs = [
        "IndexJar.java",
        "PerJarIndexResults.java",
    ],
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd:__pkg__",
        "//tests/com/github/bazelbuild/rules_jvm_external/jar:__pkg__",
    ],
    deps = [
        artifact(
            "com.google.code.gson:gson",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_binary(
    name = "IndexJar",
    main_class = "com.github.bazelbuild.rules_jvm_external.jar.IndexJar",
    visibility = ["//visibility:public"],
    runtime_deps = [
        ":IndexJar-lib",
    ],
)

java_binary(
    name = "MergeJars",
    srcs = [
        "DuplicateEntryStrategy.java",
        "MergeJars.java",
    ],
    main_class = "com.github.bazelbuild.rules_jvm_external.jar.MergeJars",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)

java_library(
    name = "create_jar_lib",
    srcs = ["CreateJar.java"],
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/javadoc:__pkg__",
        "//tests/com/github/bazelbuild/rules_jvm_external/jar:__pkg__",
        "//tests/com/github/bazelbuild/rules_jvm_external/javadoc:__pkg__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)

java_binary(
    name = "CreateJar",
    main_class = "com.github.bazelbuild.rules_jvm_external.jar.CreateJar",
    visibility = [
        "//visibility:public",
    ],
    runtime_deps = [
        ":create_jar_lib",
    ],
)
