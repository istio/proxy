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

"""Exports the public `swift_symbol_graph_aspect` aspect."""

load(
    "//swift/internal:swift_symbol_graph_aspect.bzl",
    "make_swift_symbol_graph_aspect",
)

swift_symbol_graph_aspect = make_swift_symbol_graph_aspect(
    default_emit_extension_block_symbols = "0",
    default_minimum_access_level = "public",
    doc = """\
Extracts symbol graphs from Swift modules in the build graph.
This aspect propagates a `SwiftSymbolGraphInfo` provider on any target to which
it is applied. This provider will contain the transitive module graph
information for the target's dependencies, and if the target propagates Swift
modules via its `SwiftInfo` provider, it will also extract and propagate their
symbol graphs by invoking the `swift-symbolgraph-extract` tool.
For an example of how to apply this to a custom rule, refer to the
implementation of `swift_extract_symbol_graph`.
    """,
    testonly_targets = False,
)
