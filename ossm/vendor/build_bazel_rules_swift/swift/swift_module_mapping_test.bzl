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

"""Implementation of the `swift_module_mapping_test` rule."""

load("//swift/internal:providers.bzl", "SwiftModuleAliasesInfo")
load(":providers.bzl", "SwiftInfo")

_SwiftModulesToValidateMappingInfo = provider(
    doc = "Propagates module names to have their mapping validated.",
    fields = {
        "module_names": """\
A `depset` containing the names of non-system Swift modules that should be
validated against a module mapping.
""",
    },
)

def _swift_module_mapping_test_module_collector_impl(target, aspect_ctx):
    deps = (
        getattr(aspect_ctx.rule.attr, "deps", []) +
        getattr(aspect_ctx.rule.attr, "private_deps", [])
    )

    direct_module_names = []
    transitive_module_names = [
        dep[_SwiftModulesToValidateMappingInfo].module_names
        for dep in deps
        if _SwiftModulesToValidateMappingInfo in dep
    ]

    if SwiftInfo in target:
        for module_context in target[SwiftInfo].direct_modules:
            # Ignore system modules and non-Swift modules, which aren't expected
            # to be/cannot be aliased.
            if module_context.is_system:
                continue

            swift_module = module_context.swift
            if not swift_module:
                continue

            # Collect the original module name if it is present; otherwise,
            # collect the regular module name (which is the original name when
            # the mapping isn't applied). This ensures that the test isn't
            # dependent on whether or not the module mapping flag is enabled.
            direct_module_names.append(
                swift_module.original_module_name or module_context.name,
            )

    return [
        _SwiftModulesToValidateMappingInfo(
            module_names = depset(
                direct_module_names,
                transitive = transitive_module_names,
            ),
        ),
    ]

_swift_module_mapping_test_module_collector = aspect(
    attr_aspects = [
        "deps",
        "private_deps",
    ],
    implementation = _swift_module_mapping_test_module_collector_impl,
    provides = [_SwiftModulesToValidateMappingInfo],
)

def _swift_module_mapping_test_impl(ctx):
    aliases = ctx.attr.mapping[SwiftModuleAliasesInfo].aliases
    excludes = ctx.attr.exclude
    unaliased_dep_modules = {}

    for dep in ctx.attr.deps:
        label = str(dep.label)
        dep_modules = dep[_SwiftModulesToValidateMappingInfo].module_names
        for module_name in dep_modules.to_list():
            if module_name in excludes:
                continue
            if module_name in aliases:
                continue

            if label not in unaliased_dep_modules:
                unaliased_dep_modules[label] = [module_name]
            else:
                unaliased_dep_modules[label].append(module_name)

    test_script = """\
#!/bin/bash
set -eu

"""

    if unaliased_dep_modules:
        test_script += "echo 'Module mapping {} is incomplete:'\n\n".format(
            ctx.attr.mapping.label,
        )
        for label, unaliased_names in unaliased_dep_modules.items():
            test_script += "echo 'The following transitive dependencies of {} are not aliased:'\n".format(label)
            for name in unaliased_names:
                test_script += "echo '    {}'\n".format(name)
            test_script += "echo\n\n"
        test_script += "exit 1\n"
    else:
        test_script += "exit 0\n"

    ctx.actions.write(
        content = test_script,
        is_executable = True,
        output = ctx.outputs.executable,
    )

    return [DefaultInfo(executable = ctx.outputs.executable)]

swift_module_mapping_test = rule(
    attrs = {
        "exclude": attr.string_list(
            default = [],
            doc = """\
A list of module names that may be in the transitive closure of `deps` but are
not required to be covered by `mapping`.
""",
            mandatory = False,
        ),
        "mapping": attr.label(
            doc = """\
The label of a `swift_module_mapping` target against which the transitive
closure of `deps` will be validated.
""",
            mandatory = True,
            providers = [[SwiftModuleAliasesInfo]],
        ),
        "deps": attr.label_list(
            allow_empty = False,
            aspects = [_swift_module_mapping_test_module_collector],
            doc = """\
A list of Swift targets whose transitive closure will be validated against the
`swift_module_mapping` target specified by `mapping`.
""",
            mandatory = True,
            providers = [[SwiftInfo]],
        ),
    },
    doc = """\
Validates that a `swift_module_mapping` target covers all the modules in the
transitive closure of a list of dependencies.

If you are building a static library or framework for external distribution and
you are using `swift_module_mapping` to rename some of the modules used by your
implementation, this rule will detect if any of your dependencies have taken on
a new dependency that you need to add to the mapping (otherwise, its symbols
would leak into your library with their original names).

When executed, this test will collect the names of all Swift modules in the
transitive closure of `deps`. System modules and modules whose names are listed
in the `exclude` attribute are omitted. Then, the test will fail if any of the
remaining modules collected are not present in the `aliases` of the
`swift_module_mapping` target specified by the `mapping` attribute.
""",
    implementation = _swift_module_mapping_test_impl,
    test = True,
)
