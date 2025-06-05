"""Tests for validating linking behavior."""

load(
    "//test/rules:action_command_line_test.bzl",
    "action_command_line_test",
)

def imported_framework_test_suite(name, tags = []):
    """Test suite for imported frameworks.

    Args:
        name: The base name to be used in things created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    action_command_line_test(
        name = "{}_disable_autolink_framework_test".format(name),
        expected_argv = [
            "-Xfrontend -disable-autolink-framework -Xfrontend framework1",
            "-Xfrontend -disable-autolink-framework -Xfrontend framework2",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/linking:bin",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
