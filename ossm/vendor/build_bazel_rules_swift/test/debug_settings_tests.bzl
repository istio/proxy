# Copyright 2020 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for debugging-related command line flags under various configs."""

load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)

DBG_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "dbg",
    "//command_line_option:features": [
        "-swift.cacheable_swiftmodules",
        "swift.debug_prefix_map",
        "-swift.file_prefix_map",
    ],
}

FILE_PREFIX_MAP_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "dbg",
    "//command_line_option:features": [
        "swift.debug_prefix_map",
        "swift.file_prefix_map",
    ],
}

CACHEABLE_DBG_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "dbg",
    "//command_line_option:features": [
        "swift.cacheable_swiftmodules",
        "swift.debug_prefix_map",
        "-swift.file_prefix_map",
    ],
}

FASTBUILD_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "fastbuild",
    "//command_line_option:features": [
        "-swift.cacheable_swiftmodules",
        "swift.debug_prefix_map",
        "-swift.file_prefix_map",
    ],
}

FASTBUILD_FULL_DI_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "fastbuild",
    "//command_line_option:features": [
        "-swift.cacheable_swiftmodules",
        "swift.debug_prefix_map",
        "-swift.file_prefix_map",
        "swift.full_debug_info",
    ],
}

OPT_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "opt",
    "//command_line_option:features": [
        "-swift.cacheable_swiftmodules",
        # This feature indicates *support*, not unconditional enablement, which
        # is why it is present for `opt` mode as well.
        "swift.debug_prefix_map",
    ],
}

CACHEABLE_OPT_CONFIG_SETTINGS = {
    "//command_line_option:compilation_mode": "opt",
    "//command_line_option:features": [
        "swift.cacheable_swiftmodules",
        "swift.debug_prefix_map",
    ],
}

dbg_action_command_line_test = make_action_command_line_test_rule(
    config_settings = DBG_CONFIG_SETTINGS,
)

file_prefix_map_command_line_test = make_action_command_line_test_rule(
    config_settings = FILE_PREFIX_MAP_CONFIG_SETTINGS,
)

cacheable_dbg_action_command_line_test = make_action_command_line_test_rule(
    config_settings = CACHEABLE_DBG_CONFIG_SETTINGS,
)

fastbuild_action_command_line_test = make_action_command_line_test_rule(
    config_settings = FASTBUILD_CONFIG_SETTINGS,
)

fastbuild_full_di_action_command_line_test = make_action_command_line_test_rule(
    config_settings = FASTBUILD_FULL_DI_CONFIG_SETTINGS,
)

opt_action_command_line_test = make_action_command_line_test_rule(
    config_settings = OPT_CONFIG_SETTINGS,
)

cacheable_opt_action_command_line_test = make_action_command_line_test_rule(
    config_settings = CACHEABLE_OPT_CONFIG_SETTINGS,
)

xcode_remap_command_line_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:compilation_mode": "dbg",
        "//command_line_option:features": [
            "swift.debug_prefix_map",
            "swift.remap_xcode_path",
        ],
    },
)

def debug_settings_test_suite(name, tags = []):
    """Test suite for serializing debugging options.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that `-c dbg` builds serialize debugging options, remap paths, and
    # have other appropriate debug flags.
    dbg_action_command_line_test(
        name = "{}_dbg_build".format(name),
        expected_argv = [
            "-DDEBUG",
            "-Xfrontend -serialize-debugging-options",
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
            "-g",
        ],
        not_expected_argv = [
            "-DNDEBUG",
            "-Xfrontend -no-serialize-debugging-options",
            "-gline-tables-only",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that the build is remapping paths with a file prefix map.
    file_prefix_map_command_line_test(
        name = "{}_file_prefix_map_build".format(name),
        expected_argv = [
            "-Xwrapped-swift=-file-prefix-pwd-is-dot",
        ],
        not_expected_argv = [
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that `-c dbg` builds with `swift.cacheable_modules` do NOT
    # serialize debugging options, but are otherwise the same as regular `dbg`
    # builds.
    cacheable_dbg_action_command_line_test(
        name = "{}_cacheable_dbg_build".format(name),
        expected_argv = [
            "-DDEBUG",
            "-Xfrontend -no-serialize-debugging-options",
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
            "-g",
        ],
        not_expected_argv = [
            "-DNDEBUG",
            "-Xfrontend -serialize-debugging-options",
            "-gline-tables-only",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that `-c fastbuild` builds serialize debugging options, remap
    # paths, and have other appropriate debug flags.
    fastbuild_action_command_line_test(
        name = "{}_fastbuild_build".format(name),
        expected_argv = [
            "-DDEBUG",
            "-Xfrontend -serialize-debugging-options",
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
            "-gline-tables-only",
        ],
        not_expected_argv = [
            "-DNDEBUG",
            "-Xfrontend -no-serialize-debugging-options",
            "-g",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that `-c fastbuild` builds with `swift.full_debug_info` use `-g`
    # instead of `-gline-tables-only` (this is required for Apple dSYM support).
    fastbuild_full_di_action_command_line_test(
        name = "{}_fastbuild_full_di_build".format(name),
        expected_argv = [
            "-DDEBUG",
            "-Xfrontend -serialize-debugging-options",
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
            "-g",
        ],
        not_expected_argv = [
            "-DNDEBUG",
            "-Xfrontend -no-serialize-debugging-options",
            "-gline-tables-only",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that `-c opt` builds do not serialize debugging options or remap
    # paths, and have appropriate flags otherwise.
    opt_action_command_line_test(
        name = "{}_opt_build".format(name),
        expected_argv = [
            "-DNDEBUG",
        ],
        not_expected_argv = [
            "-DDEBUG",
            "-Xfrontend -serialize-debugging-options",
            "-Xwrapped-swift=-debug-prefix-pwd-is-dot",
            "-g",
            "-gline-tables-only",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # Verify that `-c opt` builds do not serialize debugging options or remap
    # paths, and have appropriate flags otherwise.
    cacheable_opt_action_command_line_test(
        name = "{}_cacheable_opt_build".format(name),
        expected_argv = [
            "-DNDEBUG",
            "-Xfrontend -no-serialize-debugging-options",
        ],
        not_expected_argv = [
            "-Xfrontend -serialize-debugging-options",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    xcode_remap_command_line_test(
        name = "{}_remap_xcode_path".format(name),
        expected_argv = [
            "-debug-prefix-map",
            "__BAZEL_XCODE_DEVELOPER_DIR__=/PLACEHOLDER_DEVELOPER_DIR",
        ],
        target_compatible_with = ["@platforms//os:macos"],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
