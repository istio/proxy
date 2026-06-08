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

"""
Defines a rule that generates Swift libraries from protocol buffer sources.
"""

load(
    "@bazel_skylib//lib:dicts.bzl",
    "dicts",
)
load(
    "@rules_proto//proto:defs.bzl",
    "ProtoInfo",
)
load(
    "//proto:swift_proto_utils.bzl",
    "SwiftProtoCcInfo",
    "compile_swift_protos_for_target",
)
load("//swift:module_name.bzl", "derive_swift_module_name")
load(
    "//swift:providers.bzl",
    "SwiftInfo",
    "SwiftProtoCompilerInfo",
    "SwiftProtoInfo",
)
load("//swift:swift_common.bzl", "swift_common")

# buildifier: disable=bzl-visibility
load("//swift/internal:toolchain_utils.bzl", "use_swift_toolchain")

# buildifier: disable=bzl-visibility
load("//swift/internal:utils.bzl", "compact")

# _swift_proto_library_group_aspect

def _swift_proto_library_group_aspect_impl(target, aspect_ctx):
    # Get the module name and generate the module mappings:
    module_name = derive_swift_module_name(target.label)

    # Compile the source files to a module:
    direct_providers = compile_swift_protos_for_target(
        additional_compiler_deps = [],
        additional_swift_proto_compiler_info = {},
        attr = aspect_ctx.rule.attr,
        ctx = aspect_ctx,
        module_name = module_name,
        proto_infos = [target[ProtoInfo]],
        swift_proto_compilers = [aspect_ctx.attr._compiler],
        swift_proto_deps = aspect_ctx.rule.attr.deps,
        target_label = target.label,
    )

    return [
        direct_providers.direct_output_group_info,
        direct_providers.direct_swift_proto_cc_info,
        direct_providers.direct_swift_info,
        direct_providers.direct_swift_proto_info,
    ]

_swift_proto_library_group_aspect = aspect(
    attr_aspects = ["deps"],
    attrs = dicts.add(
        swift_common.toolchain_attrs(),
        {
            "_compiler": attr.label(
                default = Label("//proto:_swift_proto_compiler"),
                doc = """\
A `swift_proto_compiler` target (or target producing `SwiftProtoCompilerInfo`),
from which the Swift protos will be generated.
""",
                providers = [SwiftProtoCompilerInfo],
            ),
        },
    ),
    doc = """\
    Gathers all of the transitive ProtoInfo providers along the deps attribute
    """,
    fragments = ["cpp"],
    implementation = _swift_proto_library_group_aspect_impl,
    toolchains = use_swift_toolchain(),
)

# _swift_proto_compiler_transition

def _swift_proto_compiler_transition_impl(_, attr):
    return {"//proto:_swift_proto_compiler": attr.compiler}

_swift_proto_compiler_transition = transition(
    implementation = _swift_proto_compiler_transition_impl,
    inputs = [],
    outputs = ["//proto:_swift_proto_compiler"],
)

# swift_proto_library_group

def _swift_proto_library_group_impl(ctx):
    proto_target = ctx.attr.proto
    direct_output_group_info = proto_target[OutputGroupInfo]
    direct_swift_proto_cc_info = proto_target[SwiftProtoCcInfo]
    direct_swift_info = proto_target[SwiftInfo]
    direct_swift_proto_info = proto_target[SwiftProtoInfo]
    direct_files = compact(
        [module.swift.swiftdoc for module in direct_swift_info.direct_modules] +
        [module.swift.swiftinterface for module in direct_swift_info.direct_modules] +
        [module.swift.private_swiftinterface for module in direct_swift_info.direct_modules] +
        [module.swift.swiftmodule for module in direct_swift_info.direct_modules] +
        [module.swift.swiftsourceinfo for module in direct_swift_info.direct_modules],
    )

    return [
        DefaultInfo(
            files = depset(
                direct_files,
                transitive = [direct_swift_proto_info.pbswift_files],
            ),
        ),
        direct_output_group_info,
        direct_swift_info,
        direct_swift_proto_cc_info.cc_info,
        direct_swift_proto_cc_info.objc_info,
        direct_swift_proto_info,
    ]

swift_proto_library_group = rule(
    attrs = {
        "_allowlist_function_transition": attr.label(
            default = Label(
                "@bazel_tools//tools/allowlists/function_transition_allowlist",
            ),
        ),
        "compiler": attr.label(
            default = Label("//proto/compilers:swift_proto"),
            doc = """\
A `swift_proto_compiler` target (or target producing `SwiftProtoCompilerInfo`),
from which the Swift protos will be generated.
""",
            providers = [SwiftProtoCompilerInfo],
        ),
        "proto": attr.label(
            aspects = [_swift_proto_library_group_aspect],
            doc = """\
Exactly one `proto_library` target (or target producing `ProtoInfo`),
from which the Swift source files should be generated.
""",
            providers = [ProtoInfo],
            mandatory = True,
        ),
    },
    cfg = _swift_proto_compiler_transition,
    doc = """\
Generates a collection of Swift static library from a target producing `ProtoInfo` and its dependencies.

This rule is intended to facilitate migration from the deprecated swift proto library rules to the new ones.
Unlike `swift_proto_library` which is a drop-in-replacement for `swift_library`,
this rule uses an aspect over the direct proto library dependency and its transitive dependencies,
compiling each into a swift static library.

For example, in the following targets, the `proto_library_group_swift_proto` target only depends on
`package_2_proto` target, and this transitively depends on `package_1_proto`.

When used as a dependency from a `swift_library` or `swift_binary` target,
two modules generated from these proto library targets are visible.

Because these are derived from the proto library targets via an aspect, though,
you cannot customize many of the attributes including the swift proto compiler targets or
the module names. The module names are derived from the proto library names.

In this case, the module names are:
```
import examples_xplatform_proto_library_group_package_1_package_1_proto
import examples_xplatform_proto_library_group_package_2_package_2_proto
```

For this reason, we would encourage new consumers of the proto rules to use
`swift_proto_library` when possible.

```python
proto_library(
    name = "package_1_proto",
    srcs = glob(["*.proto"]),
    visibility = ["//visibility:public"],
)

...

proto_library(
    name = "package_2_proto",
    srcs = glob(["*.proto"]),
    visibility = ["//visibility:public"],
    deps = ["//examples/xplatform/proto_library_group/package_1:package_1_proto"],
)

...

swift_proto_library_group(
    name = "proto_library_group_swift_proto",
    proto = "//examples/xplatform/proto_library_group/package_2:package_2_proto",
)

...

swift_binary(
    name = "proto_library_group_example",
    srcs = ["main.swift"],
    deps = [
        ":proto_library_group_swift_proto",
    ],
)
```
""",
    fragments = ["cpp"],
    implementation = _swift_proto_library_group_impl,
)
