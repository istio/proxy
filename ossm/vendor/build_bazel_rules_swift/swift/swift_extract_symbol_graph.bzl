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

"""Implementation of the `swift_extract_module_graph` rule."""

load(":providers.bzl", "SwiftInfo", "SwiftSymbolGraphInfo")
load(":swift_symbol_graph_aspect.bzl", "swift_symbol_graph_aspect")

def _swift_extract_symbol_graph_impl(ctx):
    actions = ctx.actions

    output_dir = actions.declare_directory(
        "{}.symbolgraphs".format(ctx.label.name),
    )

    args = actions.args()
    args.add(output_dir.path)

    seen_modules = {}
    inputs = []

    for target in ctx.attr.targets:
        if SwiftSymbolGraphInfo in target:
            symbol_graph_info = target[SwiftSymbolGraphInfo]
            for symbol_graph in symbol_graph_info.direct_symbol_graphs:
                if symbol_graph.module_name in seen_modules:
                    fail("Module '{}' was provided by multiple targets.".format(
                        symbol_graph.module_name,
                    ))

                seen_modules[symbol_graph.module_name] = True

                # Expand the directory's files into the argument list so that
                # they are individually copied into output directory (resulting
                # in a flat layout).
                args.add_all(
                    [symbol_graph.symbol_graph_dir],
                    expand_directories = True,
                )
                inputs.append(symbol_graph.symbol_graph_dir)

    actions.run_shell(
        arguments = [args],
        command = """\
set -e
output_dir="$1"
mkdir -p "${output_dir}"
shift
for symbol_file in "$@"; do
  cp "${symbol_file}" "${output_dir}/$(basename ${symbol_file})"
done
""",
        inputs = inputs,
        mnemonic = "SwiftCollectSymbolGraphs",
        outputs = [output_dir],
    )

    return [DefaultInfo(files = depset([output_dir]))]

swift_extract_symbol_graph = rule(
    attrs = {
        # TODO: use `attr.bool` once https://github.com/bazelbuild/bazel/issues/22809 is resolved.
        "emit_extension_block_symbols": attr.string(
            default = "0",
            doc = """\
Defines if the symbol graph information for `extension` blocks should be
emitted in addition to the default symbol graph information.

This value must be either `"0"` or `"1"`.When the value is `"1"`, the symbol
graph information for `extension` blocks will be emitted in addition to
the default symbol graph information. The default value is `"0"`.
""",
            values = [
                "0",
                "1",
            ],
        ),
        "minimum_access_level": attr.string(
            default = "public",
            doc = """\
The minimum access level of the declarations that should be emitted in the
symbol graphs.

This value must be either `fileprivate`, `internal`, `private`, or `public`. The
default value is `public`.
""",
            values = [
                "fileprivate",
                "internal",
                "private",
                "public",
            ],
        ),
        "targets": attr.label_list(
            allow_empty = False,
            aspects = [swift_symbol_graph_aspect],
            doc = """\
One or more Swift targets from which to extract symbol graphs.
""",
            mandatory = True,
            providers = [[SwiftInfo]],
        ),
    },
    doc = """\
Extracts symbol graphs from one or more Swift targets.

The output of this rule is a single directory named
`${TARGET_NAME}.symbolgraphs` that contains the symbol graph JSON files for the
Swift modules directly compiled by all the targets listed in the `targets`
attribute (but not their transitive dependencies). Therefore, for each module
the directory will contain:

*   One or more files named `${MODULE_NAME}.symbols.json` containing the symbol
    graphs for non-`extension` declarations in each module.
*   Optionally, one or more files named
    `${MODULE_NAME}@${EXTENDED_MODULE}.symbols.json` containing the symbol
    graphs for declarations in each module that extend types from other modules.

This rule can be used as a simple interface to extract symbol graphs for later
processing by other Bazel rules (for example, a `genrule` that operates on the
resulting JSON files). For more complex workflows, we recommend writing a custom
rule that applies `swift_symbol_graph_aspect` to the targets of interest and
registers other Starlark actions that read the symbol graphs based on the
`SwiftSymbolGraphInfo` providers attached to those targets. The implementation
of this rule can serve as a guide for implementing such a rule.
""",
    implementation = _swift_extract_symbol_graph_impl,
)
