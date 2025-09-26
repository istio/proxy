"""Tests for derived files related command line flags under various configs."""

load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)
load("//test/rules:provider_test.bzl", "make_provider_test_rule")

default_no_split_test = make_action_command_line_test_rule()
default_no_split_provider_test = make_provider_test_rule()
split_swiftmodule_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_provider_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_wmo_test = make_action_command_line_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_wmo_provider_test = make_provider_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_skip_function_bodies_test = make_action_command_line_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
            "swift.enable_skip_function_bodies",
        ],
    },
)
default_no_split_no_emit_swiftdoc_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "-swift.emit_swiftdoc",
        ],
    },
)
default_no_split_no_emit_swiftsourceinfo_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "-swift.emit_swiftsourceinfo",
        ],
    },
)
split_no_emit_swiftdoc_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "-swift.emit_swiftdoc",
            "swift.split_derived_files_generation",
        ],
    },
)
split_no_emit_swiftsourceinfo_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "-swift.emit_swiftsourceinfo",
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_indexing_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.index_while_building",
            "swift.split_derived_files_generation",
        ],
    },
)
split_swiftmodule_bitcode_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
            "swift.emit_bc",
        ],
    },
)
split_swiftmodule_copts_test = make_action_command_line_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-DHELLO",
        ],
        "//command_line_option:objccopt": [
            "-DWORLD=1",
        ],
        "//command_line_option:features": [
            "swift.split_derived_files_generation",
        ],
    },
)

def split_derived_files_test_suite(name, tags = []):
    """Test suite for split derived files options.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    default_no_split_test(
        name = "{}_default_no_split_args".format(name),
        expected_argv = [
            "-emit-module-path",
            "-emit-object",
            "-enable-batch-mode",
            "simple.output_file_map.json",
        ],
        not_expected_argv = [
            "simple.derived_output_file_map.json",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_provider_test(
        name = "{}_default_no_split_provider_swiftmodule".format(name),
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftmodule",
        ],
        field = "direct_modules.swift.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_provider_test(
        name = "{}_default_no_split_provider_swiftdoc".format(name),
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftdoc",
        ],
        field = "direct_modules.swift.swiftdoc",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_provider_test(
        name = "{}_default_no_split_provider_swiftsourceinfo".format(name),
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftsourceinfo",
        ],
        field = "direct_modules.swift.swiftsourceinfo",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_no_emit_swiftdoc_test(
        name = "{}_default_no_split_provider_no_emit_swiftdoc".format(name),
        expected_files = [
            "-test_fixtures_debug_settings_simple.swiftdoc",
        ],
        field = "direct_modules.swift.swiftdoc",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_no_emit_swiftsourceinfo_test(
        name = "{}_default_no_split_provider_no_emit_swiftsourceinfo".format(name),
        expected_files = [
            "-test_fixtures_debug_settings_simple.swiftsourceinfo",
        ],
        field = "direct_modules.swift.swiftsourceinfo",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_provider_test(
        name = "{}_split_provider_swiftdoc".format(name),
        field = "direct_modules.swift.swiftdoc",
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftdoc",
        ],
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_provider_test(
        name = "{}_split_provider_swiftsourceinfo".format(name),
        field = "direct_modules.swift.swiftsourceinfo",
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftsourceinfo",
        ],
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_no_emit_swiftdoc_test(
        name = "{}_split_provider_no_emit_swiftdoc".format(name),
        field = "direct_modules.swift.swiftdoc",
        expected_files = [
            "-test_fixtures_debug_settings_simple.swiftdoc",
        ],
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_no_emit_swiftsourceinfo_test(
        name = "{}_split_provider_no_emit_swiftsourceinfo".format(name),
        field = "direct_modules.swift.swiftsourceinfo",
        expected_files = [
            "-test_fixtures_debug_settings_simple.swiftsourceinfo",
        ],
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_provider_test(
        name = "{}_default_no_split_provider_ccinfo".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:macos"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    default_no_split_provider_test(
        name = "{}_default_no_split_provider_ccinfo_linux".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.pic_static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:linux"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_test(
        name = "{}_object_only".format(name),
        expected_argv = [
            "-emit-object",
            "-enable-batch-mode",
            "simple.output_file_map.json",
        ],
        mnemonic = "SwiftCompile",
        not_expected_argv = [
            "-emit-module-path",
            "simple.derived_output_file_map.json",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_test(
        name = "{}_swiftmodule_only".format(name),
        expected_argv = [
            "-emit-module-path",
            "-enable-batch-mode",
            "simple.derived_output_file_map.json",
        ],
        mnemonic = "SwiftDeriveFiles",
        not_expected_argv = [
            "-emit-object",
            "simple.output_file_map.json",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_provider_test(
        name = "{}_split_provider".format(name),
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftmodule",
        ],
        field = "direct_modules.swift.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_provider_test(
        name = "{}_split_provider_ccinfo".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:macos"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_provider_test(
        name = "{}_split_provider_ccinfo_linux".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.pic_static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:linux"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_wmo_test(
        name = "{}_object_only_wmo".format(name),
        expected_argv = [
            "-emit-object",
            "-whole-module-optimization",
        ],
        mnemonic = "SwiftCompile",
        not_expected_argv = [
            "-emit-module-path",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_wmo_test(
        name = "{}_swiftmodule_only_wmo".format(name),
        expected_argv = [
            "-emit-module-path",
            "-whole-module-optimization",
        ],
        mnemonic = "SwiftDeriveFiles",
        not_expected_argv = [
            "-emit-object",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_wmo_provider_test(
        name = "{}_split_wmo_provider".format(name),
        expected_files = [
            "test_fixtures_debug_settings_simple.swiftmodule",
        ],
        field = "direct_modules.swift.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_wmo_provider_test(
        name = "{}_split_wmo_provider_ccinfo".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:macos"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_wmo_provider_test(
        name = "{}_split_wmo_provider_ccinfo_linux".format(name),
        expected_files = [
            "libsimple.a",
        ],
        field = "linking_context.linker_inputs.libraries.pic_static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_compatible_with = ["@platforms//os:linux"],
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_skip_function_bodies_test(
        name = "{}_no_skip_function_bodies".format(name),
        expected_argv = [
            "-emit-object",
        ],
        mnemonic = "SwiftCompile",
        not_expected_argv = [
            "-experimental-skip-non-inlinable-function-bodies",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_skip_function_bodies_test(
        name = "{}_skip_function_bodies".format(name),
        expected_argv = [
            "-experimental-skip-non-inlinable-function-bodies",
        ],
        mnemonic = "SwiftDeriveFiles",
        not_expected_argv = [
            "-emit-object",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_indexing_test(
        name = "{}_object_only_indexing".format(name),
        expected_argv = [
            "-emit-object",
            "-index-store-path",
        ],
        mnemonic = "SwiftCompile",
        not_expected_argv = [
            "-emit-module-path",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_indexing_test(
        name = "{}_swiftmodule_only_indexing".format(name),
        expected_argv = [
            "-emit-module-path",
        ],
        mnemonic = "SwiftDeriveFiles",
        not_expected_argv = [
            "-emit-object",
            "-index-store-path",
        ],
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_bitcode_test(
        name = "{}_bitcode_compile".format(name),
        expected_argv = ["-emit-bc"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_bitcode_test(
        name = "{}_bitcode_derive_files".format(name),
        not_expected_argv = [
            "-emit-bc",
        ],
        mnemonic = "SwiftDeriveFiles",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_copts_test(
        name = "{}_copts_compile".format(name),
        expected_argv = [
            "-DHELLO",
            "-Xcc -DWORLD=1",
        ],
        target_compatible_with = ["@platforms//os:macos"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    split_swiftmodule_copts_test(
        name = "{}_copts_derive_files".format(name),
        expected_argv = [
            "-DHELLO",
            "-Xcc -DWORLD=1",
        ],
        target_compatible_with = ["@platforms//os:macos"],
        mnemonic = "SwiftDeriveFiles",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
