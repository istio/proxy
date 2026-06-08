# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Tests for extracting symbol graphs."""

load("//test/rules:directory_test.bzl", "directory_test")

def symbol_graphs_test_suite(name, tags = []):
    """Test suite for extracting symbol graphs.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that the `swift_extract_symbol_graph` rule produces a directory
    # output containing the correct files when the requested target is a leaf
    # module.
    directory_test(
        name = "{}_extract_rule_outputs_only_requested_target_files_if_it_is_leaf".format(name),
        expected_directories = {
            "test/fixtures/symbol_graphs/some_module_symbol_graph.symbolgraphs": [
                "SomeModule.symbols.json",
                "SomeModule@Swift.symbols.json",
            ],
        },
        tags = all_tags,
        target_under_test = "//test/fixtures/symbol_graphs:some_module_symbol_graph",
    )

    # TODO: ideally this tests the contents of the json file(s) to ensure
    # the extension block symbols are present, but requires a json content test which
    # is not yet implemented.
    #
    # Verify that the `swift_extract_symbol_graph` rule produces a directory
    # output containing the correct files when the rule additionally requests
    # to emit extension block symbols.
    directory_test(
        name = "{}_extract_rule_outputs_extension_block_symbols_files".format(name),
        expected_directories = {
            "test/fixtures/symbol_graphs/some_module_symbol_graph_with_extension_block_symbols.symbolgraphs": [
                "SomeModuleWithExtension.symbols.json",
            ],
        },
        tags = all_tags,
        target_under_test = "//test/fixtures/symbol_graphs:some_module_symbol_graph_with_extension_block_symbols",
    )

    # Verify that the `swift_extract_symbol_graph` rule produces a directory
    # output containing only the graph for the requested target and not its
    # dependencies.
    directory_test(
        name = "{}_extract_rule_outputs_only_requested_target_files_if_it_has_deps".format(name),
        expected_directories = {
            "test/fixtures/symbol_graphs/importing_module_symbol_graph.symbolgraphs": [
                "ImportingModule.symbols.json",
            ],
        },
        tags = all_tags,
        target_under_test = "//test/fixtures/symbol_graphs:importing_module_symbol_graph",
    )

    # Verify that the `swift_extract_symbol_graph` rule produces a directory
    # output containing the correct files when multiple targets are requested.
    directory_test(
        name = "{}_extract_rule_outputs_all_requested_target_files".format(name),
        expected_directories = {
            "test/fixtures/symbol_graphs/all_symbol_graphs.symbolgraphs": [
                "ImportingModule.symbols.json",
                "SomeModule.symbols.json",
                "SomeModule@Swift.symbols.json",
            ],
        },
        tags = all_tags,
        target_under_test = "//test/fixtures/symbol_graphs:all_symbol_graphs",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
