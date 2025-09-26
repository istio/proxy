load("//:kt_defs.bzl", "kt_jvm_export")

kt_jvm_export(
    name = "test",
    srcs = [
        "Dependency.kt",
        "Main.kt",
    ],
    maven_coordinates = "com.example:kt:1.0.0",
)

genrule(
    name = "list-deploy-env-classes",
    srcs = [
        ":test-project",
    ],
    outs = ["classes.txt"],
    cmd = "for SRC in $(SRCS); do jar tf $$SRC >> $@; done",
)

sh_test(
    name = "check-deploy-env",
    srcs = [
        "check-deploy-env.sh",
    ],
    data = [
        ":classes.txt",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)
