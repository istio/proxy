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

"""Rule and transition to force a specific module mapping in tests."""

def _apply_mapping_transition_impl(settings, attr):
    settings = dict(settings)
    settings["//swift:module_mapping"] = attr.mapping
    return settings

apply_mapping_transition = transition(
    implementation = _apply_mapping_transition_impl,
    inputs = [],
    outputs = ["//swift:module_mapping"],
)

def _apply_mapping_impl(ctx):
    return [ctx.attr.target[0][DefaultInfo]]

apply_mapping = rule(
    attrs = {
        "mapping": attr.label(),
        "target": attr.label(cfg = apply_mapping_transition),
        "_allowlist_function_transition": attr.label(
            default = Label(
                "@bazel_tools//tools/allowlists/function_transition_allowlist",
            ),
        ),
    },
    implementation = _apply_mapping_impl,
)
