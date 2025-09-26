"""Tests for coverage-related command line flags under various configs."""

load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)

default_coverage_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:collect_code_coverage": "true",
    },
)

disabled_coverage_prefix_map_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:collect_code_coverage": "true",
        "//command_line_option:features": [
            "-swift.coverage_prefix_map",
        ],
    },
)

coverage_xcode_prefix_map_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:collect_code_coverage": "true",
        "//command_line_option:features": [
            "swift.remap_xcode_path",
        ],
    },
)

def coverage_settings_test_suite(name, tags = []):
    """Test suite for coverage options.

    Args:
        name: The base name to be used in things created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    default_coverage_test(
        name = "{}_default_coverage".format(name),
        tags = all_tags,
        expected_argv = [
            "-profile-generate",
            "-profile-coverage-mapping",
            "-Xwrapped-swift=-coverage-prefix-pwd-is-dot",
        ],
        mnemonic = "SwiftCompile",
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    disabled_coverage_prefix_map_test(
        name = "{}_prefix_map".format(name),
        tags = all_tags,
        expected_argv = [
            "-profile-generate",
            "-profile-coverage-mapping",
        ],
        not_expected_argv = [
            "-Xwrapped-swift=-coverage-prefix-pwd-is-dot",
        ],
        mnemonic = "SwiftCompile",
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    coverage_xcode_prefix_map_test(
        name = "{}_xcode_prefix_map".format(name),
        tags = all_tags,
        expected_argv = [
            "-coverage-prefix-map",
            "__BAZEL_XCODE_DEVELOPER_DIR__=/PLACEHOLDER_DEVELOPER_DIR",
        ],
        target_compatible_with = ["@platforms//os:macos"],
        mnemonic = "SwiftCompile",
        target_under_test = "//test/fixtures/debug_settings:simple",
    )
