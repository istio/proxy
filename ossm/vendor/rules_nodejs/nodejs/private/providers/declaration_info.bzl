# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""This module contains a provider for TypeScript typings (*.d.ts and package.json#typings)"""

DeclarationInfo = provider(
    doc = """The DeclarationInfo provider allows JS rules to communicate typing information.
TypeScript's .d.ts files are used as the interop format for describing types.
package.json files are included as well, as TypeScript needs to read the "typings" property.

Do not create DeclarationInfo instances directly, instead use the declaration_info factory function.

Note: historically this was a subset of the string-typed "typescript" provider.
""",
    # TODO(alexeagle): The ts_library#deps attribute should require that this provider is attached.
    # TODO: if we ever enable --declarationMap we will have .d.ts.map files too
    fields = {
        "declarations": "A depset of typings files produced by this rule",
        "transitive_declarations": """A depset of typings files produced by this rule and all its transitive dependencies.
This prevents needing an aspect in rules that consume the typings, which improves performance.""",
        "type_blocklisted_declarations": """A depset of .d.ts files that we should not use to infer JSCompiler types (via tsickle)""",
    },
)

def declaration_info(declarations, deps = []):
    """Constructs a DeclarationInfo including all transitive files needed to type-check from DeclarationInfo providers in a list of deps.

    Args:
        declarations: list of typings files
        deps: list of labels of dependencies where we should collect their DeclarationInfo to pass transitively

    Returns:
        a single DeclarationInfo provider
    """

    # TODO: add some checking actions to ensure the declarations are well-formed and don't have semantic diagnostics
    transitive_depsets = [declarations]
    for dep in deps:
        if DeclarationInfo in dep:
            transitive_depsets.append(dep[DeclarationInfo].transitive_declarations)

    return DeclarationInfo(
        declarations = declarations,
        transitive_declarations = depset(transitive = transitive_depsets),
        # Downstream ts_library rules will fail if they don't find this field
        # Even though it is only for Google Closure Compiler externs generation
        type_blocklisted_declarations = depset(),
    )
