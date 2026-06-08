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

"""A rule to collect the outputs of a `swift_library`.

This rule is used in tests to simulate "pre-built" artifacts without having to
check them in directly.
"""

load("//swift:providers.bzl", "SwiftInfo")

def _copy_file(actions, source, destination):
    """Copies the source file to the destination file.

    Args:
        actions: The object used to register actions.
        source: A `File` representing the file to be copied.
        destination: A `File` representing the destination of the copy.
    """
    args = actions.args()
    args.add(source)
    args.add(destination)

    actions.run_shell(
        arguments = [args],
        command = """\
set -e
mkdir -p "$(dirname "$2")"
cp "$1" "$2"
""",
        inputs = [source],
        outputs = [destination],
    )

def _swift_library_artifact_collector_impl(ctx):
    target = ctx.attr.target
    swift_info = target[SwiftInfo]

    if ctx.outputs.static_library:
        linker_inputs = target[CcInfo].linking_context.linker_inputs.to_list()
        lib_to_link = linker_inputs[0].libraries[0]
        _copy_file(
            ctx.actions,
            # based on build config one (but not both) of these will be present
            source = lib_to_link.static_library or lib_to_link.pic_static_library,
            destination = ctx.outputs.static_library,
        )
    if ctx.outputs.swiftdoc:
        _copy_file(
            ctx.actions,
            source = swift_info.direct_modules[0].swift.swiftdoc,
            destination = ctx.outputs.swiftdoc,
        )
    if ctx.outputs.private_swiftinterface:
        _copy_file(
            ctx.actions,
            source = swift_info.direct_modules[0].swift.private_swiftinterface,
            destination = ctx.outputs.private_swiftinterface,
        )
    if ctx.outputs.swiftinterface:
        _copy_file(
            ctx.actions,
            source = swift_info.direct_modules[0].swift.swiftinterface,
            destination = ctx.outputs.swiftinterface,
        )
    if ctx.outputs.swiftmodule:
        _copy_file(
            ctx.actions,
            source = swift_info.direct_modules[0].swift.swiftmodule,
            destination = ctx.outputs.swiftmodule,
        )
    return []

swift_library_artifact_collector = rule(
    attrs = {
        "private_swiftinterface": attr.output(mandatory = False),
        "static_library": attr.output(mandatory = False),
        "swiftdoc": attr.output(mandatory = False),
        "swiftinterface": attr.output(mandatory = False),
        "swiftmodule": attr.output(mandatory = False),
        "target": attr.label(providers = [[CcInfo, SwiftInfo]]),
    },
    implementation = _swift_library_artifact_collector_impl,
)
