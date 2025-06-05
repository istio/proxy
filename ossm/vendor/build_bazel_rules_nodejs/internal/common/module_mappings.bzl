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

# Definitions for handling path re-mapping, to support short module names.
# See pathMapping doc: https://github.com/Microsoft/TypeScript/issues/5039
#
# This reads the module_root and module_name attributes from typescript rules in
# the transitive closure, rolling these up to provide a mapping to the
# TypeScript compiler and to editors.
#

"""Helper function and aspect to get module mappings from deps
"""

def _get_deps(attrs, names):
    return [
        d
        for n in names
        if hasattr(attrs, n)
        for d in getattr(attrs, n)
    ]

# Traverse 'srcs' in addition so that we can go across a genrule
_MODULE_MAPPINGS_DEPS_NAMES = (
    ["deps", "srcs"]
)

def _debug(vars, *args):
    if "VERBOSE_LOGS" in vars.keys():
        print("[module_mappings.bzl]", *args)

def _get_module_mappings(target, ctx):
    """Returns the module_mappings from the given attrs.

    Collects a {module_name - module_root} hash from all transitive dependencies,
    checking for collisions. If a module has a non-empty `module_root` attribute,
    all sources underneath it are treated as if they were rooted at a folder
    `module_name`.

    Args:
      target: target
      ctx: ctx

    Returns:
      The module mappings
    """
    mappings = dict()
    mappings_attr = "runfiles_module_mappings"
    workspace_name = target.label.workspace_name if target.label.workspace_name else ctx.workspace_name
    all_deps = _get_deps(ctx.rule.attr, names = _MODULE_MAPPINGS_DEPS_NAMES)
    for dep in all_deps:
        if not hasattr(dep, mappings_attr):
            continue
        for k, v in getattr(dep, mappings_attr).items():
            if k in mappings and mappings[k] != v:
                fail(("duplicate module mapping at %s: %s maps to both %s and %s" %
                      (target.label, k, mappings[k], v)), "deps")
            mappings[k] = v
    if hasattr(ctx.rule.attr, "module_name") and ctx.rule.attr.module_name:
        mn = ctx.rule.attr.module_name

        # When building a mapping for use at runtime, we need paths to be relative to
        # the runfiles directory. This requires the workspace_name to be prefixed on
        # each module root.
        mr = "/".join([p for p in [workspace_name, target.label.package] if p])
        if hasattr(ctx.rule.attr, "strip_prefix") and ctx.rule.attr.strip_prefix:
            mr += "/" + ctx.rule.attr.strip_prefix
        if hasattr(ctx.rule.attr, "module_root") and ctx.rule.attr.module_root and ctx.rule.attr.module_root != ".":
            if ctx.rule.attr.module_root.endswith(".ts"):
                # Validate that sources are underneath the module root.
                # module_roots ending in .ts are a special case, they are used to
                # restrict what's exported from a build rule, e.g. only exports from a
                # specific index.d.ts file. For those, not every source must be under the
                # given module root.
                #
                # .d.ts module_root means we should be able to load in two ways:
                #   module_name -> module_path/module_root.js
                #   module_name/foo -> module_path/foo
                # So we add two mappings. The one with the trailing slash is longer,
                # so the loader should prefer it for any deep imports. The mapping
                # without the trailing slash will be used only when importing from the
                # bare module_name.
                mappings[mn + "/"] = mr + "/"
                mr = "%s/%s" % (mr, ctx.rule.attr.module_root.replace(".d.ts", ".js"))
            else:
                mr = "%s/%s" % (mr, ctx.rule.attr.module_root)
        if mn in mappings and mappings[mn] != mr:
            fail(("duplicate module mapping at %s: %s maps to both %s and %s" %
                  (target.label, mn, mappings[mn], mr)), "deps")
        mappings[mn] = mr
    _debug(ctx.var, "Mappings at %s: %s" % (target.label, mappings))
    return mappings

def _module_mappings_runtime_aspect_impl(target, ctx):
    mappings = _get_module_mappings(target, ctx)
    return struct(runfiles_module_mappings = mappings)

module_mappings_runtime_aspect = aspect(
    _module_mappings_runtime_aspect_impl,
    attr_aspects = _MODULE_MAPPINGS_DEPS_NAMES,
)
