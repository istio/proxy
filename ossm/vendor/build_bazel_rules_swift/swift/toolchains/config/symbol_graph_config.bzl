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

"""Common configuration for symbol graph extraction actions."""

load(
    "//swift/internal:action_names.bzl",
    "SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT",
)
load(":action_config.bzl", "ActionConfigInfo")

def symbol_graph_action_configs():
    """Returns the list of action configs needed to extract symbol graphs.

    If a toolchain supports symbol graph extraction, it should add these to its
    list of action configs so that those actions will be correctly configured.
    (Other required configuration is provided by `compile_action_configs`.)

    Returns:
        The list of action configs needed to extract symbol graphs.
    """
    return [
        ActionConfigInfo(
            actions = [SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT],
            configurators = [_symbol_graph_minimum_access_level_configurator],
        ),
        ActionConfigInfo(
            actions = [SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT],
            configurators = [_symbol_graph_output_configurator],
        ),
        ActionConfigInfo(
            actions = [SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT],
            configurators = [
                _symbol_graph_emit_extension_block_symbols_configurator,
            ],
        ),
    ]

def _symbol_graph_minimum_access_level_configurator(prerequisites, args):
    """Configures the minimum access level of the symbol graph extraction."""
    if prerequisites.minimum_access_level:
        args.add("-minimum-access-level", prerequisites.minimum_access_level)

def _symbol_graph_output_configurator(prerequisites, args):
    """Configures the outputs of the symbol graph extract action."""
    args.add("-output-dir", prerequisites.output_dir.path)

def _symbol_graph_emit_extension_block_symbols_configurator(prerequisites, args):
    """Configures whether `extension` block information should be emitted in the symbol graph."""

    # TODO: update to use `bool` once https://github.com/bazelbuild/bazel/issues/22809 is resolved.
    if prerequisites.emit_extension_block_symbols == "1":
        args.add("-emit-extension-block-symbols")
