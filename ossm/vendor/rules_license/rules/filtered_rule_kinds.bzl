# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Filtered rule kinds for aspect inspection.
The format of this dictionary is:

  rule_name: [attr, attr, ...]

Only filters for rules that are part of the Bazel distribution should be added
to this file. Other filters should be added in user_filtered_rule_kinds.bzl

Attributes are either the explicit list of attributes to filter, or '_*' which
would ignore all attributes prefixed with a _.
"""

# Rule kinds with attributes the aspect currently needs to ignore
aspect_filters = {
    "*": ["linter"],
    "_constant_gen": ["_generator"],
    "cc_binary": ["_*"],
    "cc_embed_data": ["_*"],
    "cc_grpc_library": ["_*"],
    "cc_library": ["_*"],
    "cc_toolchain_alias": ["_cc_toolchain"],
    "genrule": ["tools", "exec_tools", "toolchains"],
    "genyacc": ["_*"],
    "go_binary": ["_*"],
    "go_library": ["_*"],
    "go_wrap_cc": ["_*"],
    "java_binary": ["_*", "plugins", "exported_plugins"],
    "java_library": ["plugins", "exported_plugins"],
    "java_wrap_cc": ["_cc_toolchain", "swig_top"],
    "py_binary": ["_*"],
    "py_extension": ["_cc_toolchain"],
    "sh_binary": ["_bash_binary"],
}
