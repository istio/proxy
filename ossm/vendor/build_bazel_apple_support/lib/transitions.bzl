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

"""Transition support."""

def _macos_universal_transition_impl(settings, _attr):
    # Create a split transition from any macOS cpu to a list of all macOS cpus
    if settings["//command_line_option:cpu"].startswith("darwin"):
        return [
            {
                "//command_line_option:cpu": "darwin_arm64",
                "//command_line_option:platforms": [
                    settings["//platforms:macos_arm64_platform"],
                ],
            },
            {
                "//command_line_option:cpu": "darwin_x86_64",
                "//command_line_option:platforms": [
                    settings["//platforms:macos_x86_64_platform"],
                ],
            },
        ]
    else:
        return {
            "//command_line_option:cpu": settings["//command_line_option:cpu"],
            "//command_line_option:platforms": settings["//command_line_option:platforms"],
        }

macos_universal_transition = transition(
    implementation = _macos_universal_transition_impl,
    inputs = [
        "//command_line_option:cpu",
        "//command_line_option:platforms",
        "//platforms:macos_x86_64_platform",
        "//platforms:macos_arm64_platform",
    ],
    outputs = ["//command_line_option:cpu", "//command_line_option:platforms"],
)
