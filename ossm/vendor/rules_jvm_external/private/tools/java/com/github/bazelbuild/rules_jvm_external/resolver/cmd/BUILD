load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "cmd",
    srcs = glob(["*.java"]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:IndexJar-lib",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/lockfile",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/maven",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/remote",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/ui",
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
    name = "Resolver",
    main_class = "com.github.bazelbuild.rules_jvm_external.resolver.cmd.Main",
    visibility = [
        "//visibility:public",
    ],
    runtime_deps = [
        ":cmd",
    ],
)
