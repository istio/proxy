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

"""Implementation of the `swift_module_mapping` rule."""

load("//swift/internal:providers.bzl", "SwiftModuleAliasesInfo")

def _swift_module_mapping_impl(ctx):
    # This rule generates no actions/outputs; it only serves to propagate a
    # provider that other rules can read through a `label_flag` dependency.
    #
    # More specifically, this rule must never depend on the Swift toolchain,
    # because it is a dependency of the toolchain through the build flag
    # dependency.

    aliases = ctx.attr.aliases
    new_names_seen = dict()
    for original_name, new_name in aliases.items():
        previous_new_name = new_names_seen.get(new_name, None)
        if previous_new_name:
            fail((
                "Cannot alias {original} to {new}; " +
                "it was already aliased to {previous_new}"
            ).format(
                new = new_name,
                original = original_name,
                previous_new = previous_new_name,
            ))
        new_names_seen[new_name] = original_name

    return [
        SwiftModuleAliasesInfo(aliases = aliases),
    ]

swift_module_mapping = rule(
    attrs = {
        "aliases": attr.string_dict(
            doc = """\
A dictionary that remaps the names of Swift modules.

Each key in the dictionary is the name of a module as it is written in source
code. The corresponding value is the replacement module name to use when
compiling it and/or any modules that depend on it.
""",
            mandatory = True,
        ),
    },
    doc = """\
Defines a set of
[module aliases](https://github.com/apple/swift-evolution/blob/main/proposals/0339-module-aliasing-for-disambiguation.md)
that will be passed to the Swift compiler.

This rule defines a mapping from original module names to aliased names. This is
useful if you are building a library or framework for external use and want to
ensure that dependencies do not conflict with other versions of the same library
that another framework or the client may use.

To use this feature, first define a `swift_module_mapping` target that lists the
aliases you need:

```build
# //some/package/BUILD

swift_library(
    name = "Utils",
    srcs = [...],
    module_name = "Utils",
)

swift_library(
    name = "Framework",
    srcs = [...],
    module_name = "Framework",
    deps = [":Utils"],
)

swift_module_mapping(
    name = "mapping",
    aliases = {
        "Utils": "GameUtils",
    },
)
```

Then, pass the label of that target to Bazel using the
`--@build_bazel_rules_swift//swift:module_mapping` build flag:

```shell
bazel build //some/package:Framework \\
    --@build_bazel_rules_swift//swift:module_mapping=//some/package:mapping
```

When `Utils` is compiled, it will be given the module name `GameUtils` instead.
Then, when `Framework` is compiled, it will import `GameUtils` anywhere that the
source asked to `import Utils`.
""",
    implementation = _swift_module_mapping_impl,
)
