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

"""Common configuration for compile module interface actions."""

load(
    "//swift/internal:action_names.bzl",
    "SWIFT_ACTION_COMPILE_MODULE_INTERFACE",
)
load(":action_config.bzl", "ActionConfigInfo", "add_arg")

def compile_module_interface_action_configs():
    return [
        # Library evolution is implied since we've already produced a
        # .swiftinterface file. So we want to unconditionally enable the flag
        # for this action.
        ActionConfigInfo(
            actions = [SWIFT_ACTION_COMPILE_MODULE_INTERFACE],
            configurators = [add_arg("-enable-library-evolution")],
        ),
        ActionConfigInfo(
            actions = [SWIFT_ACTION_COMPILE_MODULE_INTERFACE],
            configurators = [
                _emit_module_path_from_module_interface_configurator,
            ],
        ),
        ActionConfigInfo(
            actions = [SWIFT_ACTION_COMPILE_MODULE_INTERFACE],
            configurators = [
                add_arg("-compile-module-from-interface"),
            ],
        ),
    ]

def _emit_module_path_from_module_interface_configurator(prerequisites, args):
    """Adds the `.swiftmodule` output path to the command line."""
    args.add("-o", prerequisites.swiftmodule_file)
