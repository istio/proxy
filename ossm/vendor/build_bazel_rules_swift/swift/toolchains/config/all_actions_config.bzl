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

"""Common configuration for all actions spawned by the Swift build rules."""

load("//swift/internal:action_names.bzl", "all_action_names")
load(":action_config.bzl", "ActionConfigInfo")

def _target_label_configurator(prerequisites, args):
    """Adds the Bazel target label to the action command line."""
    label = getattr(prerequisites, "target_label", None)
    if label:
        args.add(str(label), format = "-Xwrapped-swift=-bazel-target-label=%s")

def all_actions_action_configs():
    """Returns action configs that apply generally to all actions.

    This configuration function is meant for *very general* configuration, i.e.,
    flags that apply more to the worker process than the compiler.
    """
    return [
        ActionConfigInfo(
            actions = all_action_names(),
            configurators = [_target_label_configurator],
        ),
    ]
