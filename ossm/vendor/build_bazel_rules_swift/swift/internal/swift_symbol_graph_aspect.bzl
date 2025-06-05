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

"""Implementation of the `swift_symbol_graph_aspect` aspect.

The Swift build rules contain two subtly different symbol graph extraction
aspects; the public one (`swift_symbol_graph_extract`) meant for general usage,
and a private one used by `swift_test` for XCTest discovery. This file contains
the core implementation and a factory function used to create the concrete
aspects; the public aspect is exported by the `swift_symbol_graph_aspect.bzl`
file in the parent directory.
"""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("//swift:providers.bzl", "SwiftInfo", "SwiftSymbolGraphInfo")
load("//swift/internal:attrs.bzl", "swift_toolchain_attrs")
load("//swift/internal:features.bzl", "configure_features")
load("//swift/internal:symbol_graph_extracting.bzl", "extract_symbol_graph")
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)
load("//swift/internal:utils.bzl", "include_developer_search_paths")

def _swift_symbol_graph_aspect_impl(target, aspect_ctx):
    symbol_graphs = []

    if SwiftInfo in target:
        swift_toolchain = get_swift_toolchain(aspect_ctx)
        feature_configuration = configure_features(
            ctx = aspect_ctx,
            swift_toolchain = swift_toolchain,
            requested_features = aspect_ctx.features,
            unsupported_features = aspect_ctx.disabled_features,
        )

        swift_info = target[SwiftInfo]
        if CcInfo in target:
            compilation_context = target[CcInfo].compilation_context
        else:
            compilation_context = cc_common.create_compilation_context()

        emit_extension_block_symbols = aspect_ctx.attr.emit_extension_block_symbols
        minimum_access_level = aspect_ctx.attr.minimum_access_level

        for module in swift_info.direct_modules:
            output_dir = aspect_ctx.actions.declare_directory(
                "{}.symbolgraphs".format(target.label.name),
            )
            extract_symbol_graph(
                actions = aspect_ctx.actions,
                compilation_contexts = [compilation_context],
                emit_extension_block_symbols = emit_extension_block_symbols,
                feature_configuration = feature_configuration,
                include_dev_srch_paths = include_developer_search_paths(
                    aspect_ctx.rule.attr,
                ),
                minimum_access_level = minimum_access_level,
                module_name = module.name,
                output_dir = output_dir,
                swift_infos = [swift_info],
                swift_toolchain = swift_toolchain,
            )
            symbol_graphs.append(
                struct(
                    module_name = module.name,
                    symbol_graph_dir = output_dir,
                ),
            )

    # TODO(b/204480390): We intentionally don't propagate symbol graphs from
    # private deps at this time, since the main use case for them is
    # documentation. Are there use cases where we should consider this?
    transitive_symbol_graphs = []
    for dep in getattr(aspect_ctx.rule.attr, "deps", []):
        if SwiftSymbolGraphInfo in dep:
            symbol_graph_info = dep[SwiftSymbolGraphInfo]
            transitive_symbol_graphs.append(
                symbol_graph_info.transitive_symbol_graphs,
            )

    return [
        SwiftSymbolGraphInfo(
            direct_symbol_graphs = symbol_graphs,
            transitive_symbol_graphs = depset(
                symbol_graphs,
                transitive = transitive_symbol_graphs,
            ),
        ),
    ]

def _testonly_symbol_graph_aspect_impl(target, aspect_ctx):
    ignored = not getattr(aspect_ctx.rule.attr, "testonly", False)

    if ignored:
        # It's safe to return early (and not propagate transitive info) because
        # a non-`testonly` target can't depend on a `testonly` target, so there
        # is no possibility of losing anything we'd want to keep.
        return [
            SwiftSymbolGraphInfo(
                direct_symbol_graphs = [],
                transitive_symbol_graphs = depset(),
            ),
        ]

    return _swift_symbol_graph_aspect_impl(target, aspect_ctx)

def make_swift_symbol_graph_aspect(
        *,
        default_emit_extension_block_symbols,
        default_minimum_access_level,
        doc = "",
        testonly_targets):
    """Creates an aspect that extracts Swift symbol graphs from dependencies.

    Args:
        default_emit_extension_block_symbols: The default value for the `emit_extension_block_symbols`
            attribute of the aspect. A rule that applies this aspect can let users
            override this value if it also provides an attribute named `emit_extension_block_symbols`.
        default_minimum_access_level: The default minimum access level of the
            declarations that should be emitted in the symbol graphs. A rule
            that applies this aspect can let users override this value if it
            also provides an attribute named `minimum_access_level`.
        doc: The documentation string for the aspect.
        testonly_targets: If True, symbol graphs will only be extracted from
            targets that have the `testonly` attribute set.

    Returns:
        An `aspect` that can be applied to a rule's dependencies.
    """
    if testonly_targets:
        aspect_impl = _testonly_symbol_graph_aspect_impl
    else:
        aspect_impl = _swift_symbol_graph_aspect_impl

    return aspect(
        attr_aspects = ["deps"],
        attrs = dicts.add(
            swift_toolchain_attrs(),
            {
                # TODO: use `attr.bool` once https://github.com/bazelbuild/bazel/issues/22809 is resolved.
                "emit_extension_block_symbols": attr.string(
                    default = default_emit_extension_block_symbols,
                    doc = """\
Defines if the symbol graph information for `extension` blocks should be
emitted in addition to the default symbol graph information.

This value must be either `"0"` or `"1"`.When the value is `"1"`, the symbol
graph information for `extension` blocks will be emitted in addition to
the default symbol graph information. The default value is `"{default_value}"`.
""".format(
                        default_value = default_emit_extension_block_symbols,
                    ),
                    values = [
                        "0",
                        "1",
                    ],
                ),
                "minimum_access_level": attr.string(
                    default = default_minimum_access_level,
                    doc = """\
The minimum access level of the declarations that should be emitted in the
symbol graphs.

This value must be either `fileprivate`, `internal`, `private`, or `public`. The
default value is {default_value}.
""".format(
                        default_value = default_minimum_access_level,
                    ),
                    values = [
                        "fileprivate",
                        "internal",
                        "private",
                        "public",
                    ],
                ),
            },
        ),
        doc = doc,
        fragments = ["cpp"],
        implementation = aspect_impl,
        provides = [SwiftSymbolGraphInfo],
        toolchains = use_swift_toolchain(),
    )
