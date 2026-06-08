load("@rules_shell//shell:sh_test.bzl", "sh_test")

sh_test(
    name = "updater_test",
    size = "small",
    srcs = ["updater_test.sh"],
    data = [
        "//dependency:bazel-update.sh",
        "testdata/dependency_versions.json",
        "testdata/versions.bzl",
        "@jq_toolchains//:resolved_toolchain",
    ],
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    },
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)
