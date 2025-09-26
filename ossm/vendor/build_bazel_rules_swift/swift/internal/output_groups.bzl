# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Internal APIs used to compute output groups for compilation rules."""

visibility([
    "//proto/...",
    "//swift/...",
])

def supplemental_compilation_output_groups(*supplemental_outputs):
    """Computes output groups from supplemental compilation outputs.

    Args:
        *supplemental_outputs: Zero or more supplemental outputs `struct`s
            returned from calls to `compile`.

    Returns:
        A dictionary whose keys are output group names and whose values are
        depsets of `File`s, which is suitable to be `**kwargs`-expanded into an
        `OutputGroupInfo` provider.
    """
    ast_files = []
    const_values_files = []
    indexstore_files = []
    macro_expansions_files = []

    for outputs in supplemental_outputs:
        if outputs.ast_files:
            ast_files.extend(outputs.ast_files)
        if outputs.const_values_files:
            const_values_files.extend(outputs.const_values_files)
        if outputs.indexstore_directory:
            indexstore_files.append(outputs.indexstore_directory)
        if outputs.macro_expansion_directory:
            macro_expansions_files.append(outputs.macro_expansion_directory)

    output_groups = {}
    if ast_files:
        output_groups["swift_ast_file"] = depset(ast_files)
    if const_values_files:
        output_groups["const_values"] = depset(const_values_files)
    if indexstore_files:
        output_groups["swift_index_store"] = depset(indexstore_files)
    if macro_expansions_files:
        output_groups["macro_expansions"] = depset(macro_expansions_files)
    return output_groups
