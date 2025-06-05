# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Node package UMD bundling

For use by yarn_install and npm_install. Not meant to be part of the public API.
"""

load("@rules_nodejs//nodejs:providers.bzl", "DirectoryFilePathInfo", "LinkablePackageInfo")
load("//:providers.bzl", "ExternalNpmPackageInfo", "node_modules_aspect")
load("//internal/common:maybe_directory_file_path.bzl", "maybe_directory_file_path")

def _entry_point_path(ctx):
    if len(ctx.attr.entry_point.files.to_list()) > 1:
        fail("labels in entry_point must contain exactly one file")
    if len(ctx.files.entry_point) == 1:
        return ctx.files.entry_point[0].path
    if DirectoryFilePathInfo in ctx.attr.entry_point:
        return "/".join([
            ctx.attr.entry_point[DirectoryFilePathInfo].directory.path,
            ctx.attr.entry_point[DirectoryFilePathInfo].path,
        ])
    fail("entry_point must either be a file, or provide DirectoryFilePathInfo")

def _impl(ctx):
    output = ctx.actions.declare_file("%s.umd.js" % ctx.attr.package_name)

    args = ctx.actions.args()

    args.add(ctx.workspace_name)
    args.add(ctx.attr.package_name)
    args.add(_entry_point_path(ctx))
    args.add(output.path)
    args.add_joined(ctx.attr.excluded, join_with = ",")

    if ExternalNpmPackageInfo in ctx.attr.package:
        sources = ctx.attr.package[ExternalNpmPackageInfo].sources.to_list()
    else:
        sources = ctx.attr.package[LinkablePackageInfo].files.to_list()

    for s in sources:
        if s.is_directory:
            fail("npm_umd_bundle does not work with directory artifacts")

    # Only pass .js and package.json files as inputs to browserify.
    # The latter is required for module resolution in some cases.
    # We have to pass entire directories as well, since we don't know if they contain .js files
    inputs = [
        f
        for f in sources
        if f.path.endswith(".js") or f.path.endswith(".json") or f.is_directory
    ]

    ctx.actions.run(
        progress_message = "Generated UMD bundle for %s npm package [browserify]" % ctx.attr.package_name,
        executable = ctx.executable._browserify_wrapped,
        inputs = inputs,
        outputs = [output],
        arguments = [args],
    )

    return [
        DefaultInfo(files = depset([output]), runfiles = ctx.runfiles([output])),
        OutputGroupInfo(umd = depset([output])),
    ]

_ATTRS = {
    "entry_point": attr.label(
        doc = """Entry point for the npm package""",
        mandatory = True,
        allow_files = True,
    ),
    "excluded": attr.string_list(
        doc = """List of excluded packages that should not be bundled by browserify.

Packages listed here are passed to browserify with the `-u` argument. See https://github.com/browserify/browserify#usage
for details.

For example, `typeorm` npm package has an optional dependency on `react-native-sqlite-storage`. For browserify to
ignore this optional require and leave it as `require('react-native-sqlite-storage')` in the output UMD bundle, you
must specify `react-native-sqlite-storage` in the excluded attribute:

```
npm_umd_bundle(
    name = "typeorm_umd",
    package_name = "typeorm",
    entry_point = "@npm//:node_modules/typeorm/browser/index.js",
    excluded = ["react-native-sqlite-storage"],
    package = "@npm//typeorm",
)
```

This target would be then be used instead of the generated `@npm//typeorm:typeorm__umd` target in other rules.""",
    ),
    "package": attr.label(
        doc = """The npm package target""",
        mandatory = True,
        aspects = [node_modules_aspect],
    ),
    "package_name": attr.string(
        doc = """The name of the npm package""",
        mandatory = True,
    ),
    "_browserify_wrapped": attr.label(
        executable = True,
        cfg = "exec",
        default = Label("@build_bazel_rules_nodejs//internal/npm_install:browserify-wrapped"),
    ),
}

_npm_umd_bundle = rule(
    implementation = _impl,
    attrs = _ATTRS,
    outputs = {"umd": "%{package_name}.umd.js"},
    doc = """Node package umd bundling""",
)

def npm_umd_bundle(name, **kwargs):
    _npm_umd_bundle(
        name = name,
        entry_point = maybe_directory_file_path(name, kwargs.pop("entry_point", None)),
        **kwargs
    )
