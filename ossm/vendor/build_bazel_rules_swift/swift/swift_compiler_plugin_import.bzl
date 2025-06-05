# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Implementation of the `swift_compiler_plugin_import` rule."""

load("//swift/internal:providers.bzl", "SwiftCompilerPluginInfo")

def _swift_compiler_plugin_import_impl(ctx):
    return [
        SwiftCompilerPluginInfo(
            executable = ctx.executable.executable,
            module_names = depset(ctx.attr.module_names),
        ),
    ]

swift_compiler_plugin_import = rule(
    attrs = {
        "executable": attr.label(
            allow_files = True,
            cfg = "exec",
            doc = """\
The compiler plugin executable that will be passed to the Swift compiler when
compiling any modules that depend on the plugin. This attribute may refer
directly to an executable binary or to another rule that produces an executable
binary.
""",
            executable = True,
            mandatory = True,
        ),
        "module_names": attr.string_list(
            allow_empty = False,
            doc = """
The list of names of Swift modules in the plugin executable that provide
implementations of plugin types, which the compiler uses to look up their
implementations.
""",
            mandatory = True,
        ),
    },
    doc = """\
Allows for a Swift compiler plugin to be loaded from a prebuilt executable or
some other binary-propagating rule, instead of building the plugin from source
using `swift_compiler_plugin`.
""",
    implementation = _swift_compiler_plugin_import_impl,
    provides = [SwiftCompilerPluginInfo],
)
