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

"""ExternalNpmPackageInfo providers and apsect to collect node_modules from deps.
"""

# ExternalNpmPackageInfo provider is provided by targets that are external npm packages by
# `js_library` rule when package_name is set to "node_modules", as well as other targets that
# have direct or transitive deps on `js_library` targets via the `node_modules_aspect` below.
ExternalNpmPackageInfo = provider(
    doc = "Provides information about one or more external npm packages",
    fields = {
        "direct_sources": "Depset of direct source files in these external npm package(s)",
        "path": "The local workspace path that these external npm deps should be linked at. If empty, they will be linked at the root.",
        "sources": "Depset of direct & transitive source files in these external npm package(s) and transitive dependencies",
        "workspace": "The workspace name that these external npm package(s) are provided from",
    },
)

def _node_modules_aspect_impl(target, ctx):
    if ExternalNpmPackageInfo in target:
        return []

    # provide ExternalNpmPackageInfo if it is not already provided there are ExternalNpmPackageInfo deps
    providers = []

    # map of 'path' to [workspace, sources_depsets]
    paths = {}

    if hasattr(ctx.rule.attr, "deps"):
        for dep in ctx.rule.attr.deps:
            if ExternalNpmPackageInfo in dep:
                path = getattr(dep[ExternalNpmPackageInfo], "path", "")
                workspace = dep[ExternalNpmPackageInfo].workspace
                sources_depsets = []
                if path in paths:
                    path_entry = paths[path]
                    if path_entry[0] != workspace:
                        fail("All npm dependencies at the path '%s' must come from a single workspace. Found '%s' and '%s'." % (path, workspace, path_entry[0]))
                    sources_depsets = path_entry[1]
                sources_depsets.append(dep[ExternalNpmPackageInfo].sources)
                paths[path] = [workspace, sources_depsets]
        for path, path_entry in paths.items():
            providers.extend([ExternalNpmPackageInfo(
                direct_sources = depset(),
                sources = depset(transitive = path_entry[1]),
                workspace = path_entry[0],
                path = path,
            )])
    return providers

node_modules_aspect = aspect(
    _node_modules_aspect_impl,
    attr_aspects = ["deps"],
)
