"helpers for test assertions"

load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@build_bazel_rules_nodejs//:index.bzl", "npm_package_bin")

def assert_program_produces_stdout(name, tool, stdout, tags = []):
    write_file(
        name = "_write_expected_" + name,
        out = "expected_" + name,
        content = stdout,
        tags = tags,
    )

    npm_package_bin(
        name = "_write_actual_" + name,
        tool = tool,
        stdout = "actual_" + name,
        tags = tags,
    )

    diff_test(
        name = name,
        file1 = "expected_" + name,
        file2 = "actual_" + name,
        tags = tags + [
            # diff_test has line endings issues on Windows
            "fix-windows",
        ],
    )
