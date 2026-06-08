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

"""Implementation of autolink logic for Swift."""

load(
    "//swift/toolchains/config:action_config.bzl",
    "ActionConfigInfo",
    "ConfigResultInfo",
)
load(":action_names.bzl", "SWIFT_ACTION_AUTOLINK_EXTRACT")
load(":actions.bzl", "run_toolchain_action")

def _autolink_extract_input_configurator(prerequisites, args):
    """Configures the inputs of the autolink-extract action."""
    object_files = prerequisites.object_files

    args.add_all(object_files)
    return ConfigResultInfo(inputs = object_files)

def _autolink_extract_output_configurator(prerequisites, args):
    """Configures the outputs of the autolink-extract action."""
    args.add("-o", prerequisites.autolink_file)

def autolink_extract_action_configs():
    """Returns the list of action configs needed to perform autolink extraction.

    If a toolchain supports autolink extraction, it should add these to its list
    of action configs so that those actions will be correctly configured.

    Returns:
        The list of action configs needed to perform autolink extraction.
    """
    return [
        ActionConfigInfo(
            actions = [SWIFT_ACTION_AUTOLINK_EXTRACT],
            configurators = [
                _autolink_extract_input_configurator,
                _autolink_extract_output_configurator,
            ],
        ),
    ]

def register_autolink_extract_action(
        actions,
        autolink_file,
        feature_configuration,
        object_files,
        swift_toolchain):
    """Extracts autolink information from Swift `.o` files.

    For some platforms (such as Linux), autolinking of imported frameworks is
    achieved by extracting the information about which libraries are needed from
    the `.o` files and producing a text file with the necessary linker flags.
    That file can then be passed to the linker as a response file (i.e.,
    `@flags.txt`).

    Args:
        actions: The object used to register actions.
        autolink_file: A `File` into which the autolink information will be
            written.
        feature_configuration: The Swift feature configuration.
        object_files: The list of object files whose autolink information will
            be extracted.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.
    """
    prerequisites = struct(
        autolink_file = autolink_file,
        object_files = object_files,
        target_label = feature_configuration._label,
    )
    run_toolchain_action(
        actions = actions,
        action_name = SWIFT_ACTION_AUTOLINK_EXTRACT,
        feature_configuration = feature_configuration,
        outputs = [autolink_file],
        prerequisites = prerequisites,
        progress_message = "Extracting autolink data for Swift module %{label}",
        swift_toolchain = swift_toolchain,
    )
