load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//version:utils.bzl", "module_versions")

# Test: Basic module version extraction
module_versions(
    name = "test_versions",
    module_bazel = "data/test_MODULE.bazel",
    module_lock = "data/test_MODULE.bazel.lock",
)

sh_test(
    name = "module_versions_test",
    size = "small",
    srcs = ["module_versions_test.sh"],
    data = [
        ":test_versions",
        "data/expected_versions.json",
        "@jq_toolchains//:resolved_toolchain",
    ],
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    },
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)

sh_test(
    name = "version_sorting_test",
    size = "small",
    srcs = ["test_version_sorting.sh"],
    data = [
        "@jq_toolchains//:resolved_toolchain",
    ],
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    },
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)

sh_test(
    name = "new_fields_test",
    size = "small",
    srcs = ["test_new_fields.sh"],
    data = [
        ":test_versions",
        "@jq_toolchains//:resolved_toolchain",
    ],
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    },
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)
