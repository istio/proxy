# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Tests for `output_file_map`."""

load(
    "//test/rules:output_file_map_test.bzl",
    "make_output_file_map_test_rule",
    "output_file_map_test",
)

# Test with enabled `swift.add_target_name_to_output` feature
output_file_map_target_name_test = make_output_file_map_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.add_target_name_to_output",
        ],
    },
)

output_file_map_embed_bitcode_test = make_output_file_map_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.emit_bc",
        ],
    },
)

# Test with enabled `swift.add_target_name_to_output` feature
output_file_map_target_name_embed_bitcode_test = make_output_file_map_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.emit_bc",
            "swift.add_target_name_to_output",
        ],
    },
)

output_file_map_embed_bitcode_wmo_test = make_output_file_map_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
        "//command_line_option:features": [
            "swift.emit_bc",
        ],
    },
)

# Test with enabled `swift.add_target_name_to_output` feature
output_file_map_embed_target_name_bitcode_wmo_test = make_output_file_map_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
        "//command_line_option:features": [
            "swift.emit_bc",
            "swift.add_target_name_to_output",
        ],
    },
)

output_file_map_thin_lto_test = make_output_file_map_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.thin_lto",
        ],
    },
)

output_file_map_full_lto_test = make_output_file_map_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.full_lto",
        ],
    },
)

def output_file_map_test_suite(name, tags = []):
    """Test suite for `swift_library` generating output file maps.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    output_file_map_test(
        name = "{}_default".format(name),
        expected_mapping = {
            "object": "test/fixtures/debug_settings/simple_objs/Empty.swift.o",
            "const-values": "test/fixtures/debug_settings/simple_objs/Empty.swift.swiftconstvalues",
        },
        file_entry = "test/fixtures/debug_settings/Empty.swift",
        output_file_map = "test/fixtures/debug_settings/simple.output_file_map.json",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    # In Xcode13, the bitcode file needs to be in the output file map
    # (https://github.com/bazelbuild/rules_swift/issues/682).
    output_file_map_embed_bitcode_test(
        name = "{}_emit_bc".format(name),
        expected_mapping = {
            "llvm-bc": "test/fixtures/debug_settings/simple_objs/Empty.swift.bc",
        },
        file_entry = "test/fixtures/debug_settings/Empty.swift",
        output_file_map = "test/fixtures/debug_settings/simple.output_file_map.json",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    output_file_map_embed_bitcode_wmo_test(
        name = "{}_emit_bc_wmo".format(name),
        expected_mapping = {
            "llvm-bc": "test/fixtures/debug_settings/simple_objs/Empty.swift.bc",
        },
        file_entry = "test/fixtures/debug_settings/Empty.swift",
        output_file_map = "test/fixtures/debug_settings/simple.output_file_map.json",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    output_file_map_thin_lto_test(
        name = "{}_thin_lto".format(name),
        expected_mapping = {
            "llvm-bc": "test/fixtures/debug_settings/simple_objs/Empty.swift.bc",
        },
        file_entry = "test/fixtures/debug_settings/Empty.swift",
        output_file_map = "test/fixtures/debug_settings/simple.output_file_map.json",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    output_file_map_full_lto_test(
        name = "{}_full_lto".format(name),
        expected_mapping = {
            "llvm-bc": "test/fixtures/debug_settings/simple_objs/Empty.swift.bc",
        },
        file_entry = "test/fixtures/debug_settings/Empty.swift",
        output_file_map = "test/fixtures/debug_settings/simple.output_file_map.json",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
