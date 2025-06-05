"""Tests for swift_binary's output path."""

load(
    "//test/rules:swift_binary_linking_test.bzl",
    "make_swift_binary_linking_test_rule",
    "swift_binary_linking_test",
)

swift_binary_linking_with_target_name_test = make_swift_binary_linking_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.add_target_name_to_output",
        ],
    },
)

def swift_binary_linking_test_suite(name, tags = []):
    """Test suite for swift_binary's output path.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    swift_binary_linking_with_target_name_test(
        name = "{}_with_target_name".format(name),
        output_binary_path = "test/fixtures/linking/bin/bin",
        tags = all_tags,
        target_under_test = "//test/fixtures/linking:bin",
    )

    swift_binary_linking_test(
        name = "{}_default".format(name),
        output_binary_path = "test/fixtures/linking/bin",
        tags = all_tags,
        target_under_test = "//test/fixtures/linking:bin",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
