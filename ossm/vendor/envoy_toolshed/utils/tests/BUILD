load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//:utils.bzl", "json_merge")

# Test: Basic JSON merge
json_merge(
    name = "test_merge",
    srcs = [
        "data/input1.json",
        "data/input2.json",
    ],
)

# Test: YAML conversion and merge
json_merge(
    name = "test_yaml_merge",
    srcs = ["data/input1.json"],
    yaml_srcs = ["data/input.yaml"],
)

# Test: Custom filter
json_merge(
    name = "test_filter",
    srcs = [
        "data/input1.json",
        "data/input2.json",
    ],
    filter = "{\"combined\": (.[0] * .[1]), \"keys\": (.[0] | keys)}",
)

sh_test(
    name = "json_merge_test",
    size = "small",
    srcs = ["json_merge_test.sh"],
    data = [
        ":test_merge",
        ":test_yaml_merge",
        ":test_filter",
        "data/expected_merge.json",
        "data/expected_yaml_merge.json",
        "data/expected_filter.json",
        "@jq_toolchains//:resolved_toolchain",
    ],
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    },
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)
