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
Defines a rule that generates a Swift library from protocol buffer sources.
"""

load(
    "@bazel_skylib//lib:dicts.bzl",
    "dicts",
)
load(
    "@rules_proto//proto:defs.bzl",
    "ProtoInfo",
)
load("//swift:module_name.bzl", "derive_swift_module_name")
load("//swift:providers.bzl", "SwiftProtoCompilerInfo")
load("//swift:swift_clang_module_aspect.bzl", "swift_clang_module_aspect")
load("//swift:swift_common.bzl", "swift_common")

# buildifier: disable=bzl-visibility
load("//swift/internal:attrs.bzl", "swift_deps_attr")

# buildifier: disable=bzl-visibility
load("//swift/internal:toolchain_utils.bzl", "use_swift_toolchain")

# buildifier: disable=bzl-visibility
load("//swift/internal:utils.bzl", "compact")
load(
    ":swift_proto_utils.bzl",
    "compile_swift_protos_for_target",
)

# Private

def _get_module_name(attr, target_label):
    """Gets the module name from the given attributes and target label.

    Uses the module name from the attribute if provided,
    or failing this, falls back to the derived module name.
    """
    module_name = attr.module_name
    if not module_name:
        module_name = derive_swift_module_name(target_label)
    return module_name

# Rule

def _swift_proto_library_impl(ctx):
    # Get the module name and generate the module mappings:
    module_name = _get_module_name(ctx.attr, ctx.label)

    # Compile the source files to a module:
    direct_providers = compile_swift_protos_for_target(
        additional_compiler_deps = ctx.attr.additional_compiler_deps,
        additional_swift_proto_compiler_info = ctx.attr.additional_compiler_info,
        attr = ctx.attr,
        ctx = ctx,
        module_name = module_name,
        proto_infos = [d[ProtoInfo] for d in ctx.attr.protos],
        swift_proto_compilers = ctx.attr.compilers,
        swift_proto_deps = ctx.attr.deps,
        target_label = ctx.label,
    )
    direct_output_group_info = direct_providers.direct_output_group_info
    direct_swift_proto_cc_info = direct_providers.direct_swift_proto_cc_info
    direct_swift_info = direct_providers.direct_swift_info
    direct_swift_proto_info = direct_providers.direct_swift_proto_info
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
            runfiles = ctx.runfiles(
                collect_data = True,
                collect_default = True,
                files = ctx.files.data,
            ),
        ),
        direct_output_group_info,
        direct_swift_info,
        direct_swift_proto_cc_info.cc_info,
        direct_swift_proto_cc_info.objc_info,
        direct_swift_proto_info,
    ]

swift_proto_library = rule(
    attrs = dicts.add(
        swift_common.library_rule_attrs(
            additional_deps_aspects = [
                swift_clang_module_aspect,
            ],
            requires_srcs = False,
        ),
        {
            "protos": attr.label_list(
                doc = """\
A list of `proto_library` targets (or targets producing `ProtoInfo`),
from which the Swift source files should be generated.
""",
                providers = [ProtoInfo],
            ),
            "compilers": attr.label_list(
                default = ["//proto/compilers:swift_proto"],
                doc = """\
One or more `swift_proto_compiler` targets (or targets producing `SwiftProtoCompilerInfo`),
from which the Swift protos will be generated.
""",
                providers = [SwiftProtoCompilerInfo],
            ),
            "additional_compiler_deps": swift_deps_attr(
                aspects = [
                    swift_clang_module_aspect,
                ],
                default = [],
                doc = """\
List of additional dependencies required by the generated Swift code at compile time,
whose SwiftProtoInfo will be ignored.
""",
            ),
            "additional_compiler_info": attr.string_dict(
                default = {},
                doc = """\
Dictionary of additional information passed to the compiler targets.
See the documentation of the respective compiler rules for more information
on which fields are accepted and how they are used.
""",
            ),
        },
    ),
    doc = """\
Generates a Swift static library from one or more targets producing `ProtoInfo`.

```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "foo",
    srcs = ["foo.proto"],
)

swift_proto_library(
    name = "foo_swift",
    protos = [":foo"],
)
```

If your protos depend on protos from other targets, add dependencies between the
swift_proto_library targets which mirror the dependencies between the proto targets.

```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "bar",
    srcs = ["bar.proto"],
    deps = [":foo"],
)

swift_proto_library(
    name = "bar_swift",
    protos = [":bar"],
    deps = [":foo_swift"],
)
```
""",
    fragments = ["cpp"],
    implementation = _swift_proto_library_impl,
    toolchains = use_swift_toolchain(),
)
