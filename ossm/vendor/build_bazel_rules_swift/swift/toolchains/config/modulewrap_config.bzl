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

"""Common configuration for modulewrap actions."""

load("//swift/internal:action_names.bzl", "SWIFT_ACTION_MODULEWRAP")
load(":action_config.bzl", "ActionConfigInfo", "ConfigResultInfo")

def modulewrap_action_configs():
    """Returns the list of action configs needed to perform module wrapping.

    If a toolchain supports module wrapping, it should add these to its list of
    action configs so that those actions will be correctly configured.

    Returns:
        The list of action configs needed to perform module wrapping.
    """
    return [
        ActionConfigInfo(
            actions = [SWIFT_ACTION_MODULEWRAP],
            configurators = [
                _modulewrap_input_configurator,
                _modulewrap_output_configurator,
            ],
        ),
    ]

def _modulewrap_input_configurator(prerequisites, args):
    """Configures the inputs of the modulewrap action."""
    swiftmodule_file = prerequisites.swiftmodule_file

    args.add(swiftmodule_file)
    return ConfigResultInfo(inputs = [swiftmodule_file])

def _modulewrap_output_configurator(prerequisites, args):
    """Configures the outputs of the modulewrap action."""
    args.add("-o", prerequisites.object_file)
