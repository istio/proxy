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

"""Implementation of the `swift_cross_import_overlay` rule."""

load("//swift/internal:providers.bzl", "SwiftCrossImportOverlayInfo")
load(":providers.bzl", "SwiftInfo")

def _get_sole_module_name(swift_info, attr):
    if len(swift_info.direct_modules) != 1:
        fail(("The target specified by '{}' must define exactly one Swift " +
              "and/or Clang module.").format(attr))
    return swift_info.direct_modules[0].name

def _swift_cross_import_overlay_impl(ctx):
    bystanding_module = _get_sole_module_name(
        ctx.attr.bystanding_module[SwiftInfo],
        "bystanding_module",
    )
    declaring_module = _get_sole_module_name(
        ctx.attr.declaring_module[SwiftInfo],
        "declaring_module",
    )
    return [
        SwiftCrossImportOverlayInfo(
            bystanding_module = bystanding_module,
            declaring_module = declaring_module,
            swift_infos = [dep[SwiftInfo] for dep in ctx.attr.deps],
        ),
    ]

swift_cross_import_overlay = rule(
    attrs = {
        "bystanding_module": attr.label(
            doc = """
A label for the target representing the second of the two modules (the other
being `declaring_module`) that must be imported for the cross-import overlay
modules to be imported. It is completely passive in the cross-import process,
having no definition with or other association to either the declaring module or
the cross-import modules.
""",
            mandatory = True,
            providers = [[SwiftInfo]],
        ),
        "declaring_module": attr.label(
            doc = """\
A label for the target representing the first of the two modules (the other
being `bystanding_module`) that must be imported for the cross-import overlay
modules to be imported. This is the module that contains the `.swiftcrossimport`
overlay definition that connects it to the bystander and to the overlay modules.
""",
            mandatory = True,
            providers = [[SwiftInfo]],
        ),
        "deps": attr.label_list(
            allow_empty = False,
            doc = """\
A non-empty list of targets representing modules that should be passed as
dependencies when a target depends on both `declaring_module` and
`bystanding_module`.
""",
            mandatory = True,
            providers = [[SwiftInfo]],
        ),
    },
    doc = """\
Declares a cross-import overlay that will be automatically added as a dependency
by the toolchain if its declaring and bystanding modules are both imported.

Since Bazel requires the dependency graph to be explicit, cross-import overlays
do not work correctly when the Swift compiler attempts to import them
automatically when they aren't represented in the graph. Users can explicitly
depend on the cross-import overlay module, but this is unsatisfying because
there is no single `import` declaration in the source code that indicates what
needs to be depended on.

To address this, the toolchain owner can define a `swift_cross_import_overlay`
target for each cross-import overlay that they wish to support and set them as
`cross_import_overlays` on the toolchain. During Swift compilation analysis, the
direct dependencies will be scanned and if any pair of dependencies matches a
cross-import overlay defined by the toolchain, the overlay module will be
automatically injected as a dependency as well.

NOTE: This rule and its associated APIs only exists to support cross-import
overlays _already defined by Apple's SDKs_. Since cross-import overlays are not
a public feature of the compiler and its design and implementation may change in
the future, this rule is not recommended for other widespread use.
""",
    implementation = _swift_cross_import_overlay_impl,
)
