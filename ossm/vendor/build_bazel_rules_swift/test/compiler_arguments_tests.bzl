"""Tests for various compiler arguments."""

load(
    "//test/rules:action_command_line_test.bzl",
    "action_command_line_test",
    "make_action_command_line_test_rule",
)

split_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)

thin_lto_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.thin_lto",
        ],
    },
)

full_lto_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.full_lto",
        ],
    },
)

def compiler_arguments_test_suite(name, tags = []):
    """Test suite for various command line flags passed to Swift compiles.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    action_command_line_test(
        name = "{}_no_package_by_default".format(name),
        not_expected_argv = ["-package-name"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:no_package_name",
    )

    action_command_line_test(
        name = "{}_lib_with_package".format(name),
        expected_argv = ["-package-name lib"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:lib_package_name",
    )

    action_command_line_test(
        name = "{}_bin_with_package".format(name),
        expected_argv = ["-package-name bin"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:bin_package_name",
    )

    action_command_line_test(
        name = "{}_test_with_package".format(name),
        expected_argv = ["-package-name test"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:test_package_name",
    )

    split_test(
        name = "{}_split_lib_with_package".format(name),
        expected_argv = ["-package-name lib"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:lib_package_name",
    )

    split_test(
        name = "{}_split_module_with_package".format(name),
        expected_argv = ["-package-name lib"],
        mnemonic = "SwiftDeriveFiles",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:lib_package_name",
    )

    thin_lto_test(
        name = "{}_thin_lto".format(name),
        expected_argv = ["-lto=llvm-thin"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:bin",
    )

    full_lto_test(
        name = "{}_full_lto".format(name),
        expected_argv = ["-lto=llvm-full"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/compiler_arguments:bin",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
