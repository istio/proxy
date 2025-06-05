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

"""npm_link is used to add a LinkablePackageInfo to a target that doesn't
provide one or modify the package_path of a target which provides LinkablePackageInfo.

The rule will not allow you to change the package_name of target that already
provides a LinkablePackageInfo. If a target already provides a package_name, the
package_name set here must match.
"""

load(
    "@rules_nodejs//nodejs:providers.bzl",
    "DeclarationInfo",
    "JSModuleInfo",
    "LinkablePackageInfo",
)
load(
    "//:providers.bzl",
    "ExternalNpmPackageInfo",
    "JSNamedModuleInfo",
)
load(
    "//third_party/github.com/bazelbuild/bazel-skylib:rules/private/copy_file_private.bzl",
    "copy_bash",
    "copy_cmd",
)

_ATTRS = {
    "deps": attr.label_list(
        mandatory = True,
    ),
    "is_windows": attr.bool(
        doc = "Internal use only. Automatically set by macro",
        mandatory = True,
    ),
    "package_name": attr.string(
        mandatory = True,
    ),
    "package_path": attr.string(),
}

def _impl(ctx):
    if len(ctx.attr.deps) != 1:
        fail("Expected a single target")

    link_target = ctx.attr.deps[0]

    if ExternalNpmPackageInfo in link_target:
        fail("3rd party dependency '%s' with ExternalNpmPackageInfo cannot be used as an npm_link target" % str(link_target))

    if LinkablePackageInfo in link_target:
        path = link_target[LinkablePackageInfo].path
        files = link_target[LinkablePackageInfo].files.to_list()
    else:
        # link to the bazel-out of this npm_link package since the link target is not a js_library and we want
        # to ensure all files are linkable in bazel-out
        path = "/".join([p for p in [ctx.bin_dir.path, ctx.label.workspace_root, ctx.label.package] if p])
        files = []
        for file in link_target[DefaultInfo].files.to_list():
            # copy all files into bin in this case
            if not file.path.startswith("external/"):
                dirname = file.dirname
                if not file.is_source:
                    if not file.dirname.startswith(ctx.bin_dir.path + "/"):
                        fail("Generated file %s does not start with ctx.bin_dir.path %s" % (file, ctx.bin_dir.path))
                    dirname = dirname[len(ctx.bin_dir.path) + 1:]
                if link_target.label.package:
                    if dirname != link_target.label.package and not dirname.startswith(link_target.label.package + "/"):
                        fail("File %s is not under target package dir %s" % (file, link_target.label.package))
                    dirname = dirname[len(link_target.label.package) + 1:]
                file_path = "/".join([p for p in [dirname, file.basename] if p])
                dst = ctx.actions.declare_file(file_path)
                if ctx.attr.is_windows:
                    copy_cmd(ctx, file, dst)
                else:
                    copy_bash(ctx, file, dst)

                # re-assign file to the one now copied into the bin folder
                file = dst
            files.append(file)
    providers = [
        LinkablePackageInfo(
            package_name = ctx.attr.package_name,
            package_path = ctx.attr.package_path,
            path = path,
            files = depset(files),
        ),
    ]

    # Forward other providers that linkable targets such as link_target may provide
    # See js_library rule which provides all of these for example.
    if DeclarationInfo in link_target:
        providers.append(link_target[DeclarationInfo])
    if JSModuleInfo in link_target:
        providers.append(link_target[JSModuleInfo])
    if JSNamedModuleInfo in link_target:
        providers.append(link_target[JSNamedModuleInfo])

    # Ensure files are runfiles
    if DefaultInfo in link_target:
        providers.append(DefaultInfo(
            files = depset(files, transitive = [link_target[DefaultInfo].files]),
            runfiles = ctx.runfiles(files = files).merge(link_target[DefaultInfo].data_runfiles),
        ))
    else:
        providers.append(DefaultInfo(
            files = depset(files),
            runfiles = ctx.runfiles(files = files),
        ))

    return providers

_npm_link = rule(
    implementation = _impl,
    attrs = _ATTRS,
)

def npm_link(name, target, package_name, package_path = "", **kwargs):
    """Adapts a target by forwarding its providers and LinkablePackageInfo with the specified package_path.

If the specified target already exports a LinkablePackageInfo with the specified package_name.

If the specified targert exports none of the above, a LinkablePackageInfo will be exported with the path
set to the target's package & the files provided from the targets DefaultInfo.

    Args:
        name: The name for the target
        target: The target to adapt by
        package_name: The name it will be imported by
            If package_name is set on target this must match the taget's package_name.
        package_path: The directory in the workspace to link to
            "" will link to root of the workspace.
        **kwargs: used for undocumented legacy features
    """
    _npm_link(
        name = name,
        # pass to rule as deps so aspects can walk still walk the deps tree through this rule
        deps = [target],
        package_name = package_name,
        package_path = package_path,
        is_windows = select({
            "@bazel_tools//src/conditions:host_windows": True,
            "//conditions:default": False,
        }),
        **kwargs
    )
