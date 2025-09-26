"""Tests for validating @main related usage."""

load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)

mainattr_test = make_action_command_line_test_rule()

def mainattr_test_suite(name, tags = []):
    """Test suite validating `@main` support.

    Args:
        name: The base name to be used in things created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    mainattr_test(
        name = "{}_single_main".format(name),
        not_expected_argv = ["-parse-as-library"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/mainattr:main",
    )

    mainattr_test(
        name = "{}_single_custom_main".format(name),
        expected_argv = ["-parse-as-library"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/mainattr:custommain",
    )

    mainattr_test(
        name = "{}_multiple_files".format(name),
        not_expected_argv = ["-parse-as-library"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/mainattr:multiplefiles",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
