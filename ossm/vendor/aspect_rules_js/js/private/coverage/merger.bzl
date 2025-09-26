"Internal use only"

# Simple binary that call coverage.js with node toolchain
load("@aspect_bazel_lib//lib:windows_utils.bzl", "create_windows_native_launcher_script")
load("//js/private:bash.bzl", "BASH_INITIALIZE_RUNFILES")

_ATTRS = {
    "entry_point": attr.label(default = Label("//js/private/coverage:coverage.js"), allow_single_file = [".js"]),
    "_launcher_template": attr.label(
        default = Label("//js/private/coverage:coverage.sh.tpl"),
        allow_single_file = True,
    ),
    "_windows_constraint": attr.label(default = "@platforms//os:windows"),
}

# Do the opposite of _to_manifest_path in
# https://github.com/bazelbuild/rules_nodejs/blob/8b5d27400db51e7027fe95ae413eeabea4856f8e/nodejs/toolchain.bzl#L50
# to get back to the short_path.
# TODO(3.0): remove this after a grace period for the DEPRECATED toolchain attributes
# buildifier: disable=unused-variable
def _deprecated_target_tool_path_to_short_path(tool_path):
    return ("../" + tool_path[len("external/"):]) if tool_path.startswith("external/") else tool_path

def _coverage_merger_impl(ctx):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])
    nodeinfo = ctx.toolchains["@rules_nodejs//nodejs:toolchain_type"].nodeinfo

    if hasattr(nodeinfo, "node"):
        node_path = nodeinfo.node.short_path if nodeinfo.node else nodeinfo.node_path
    else:
        # TODO(3.0): drop support for deprecated toolchain attributes
        node_path = _deprecated_target_tool_path_to_short_path(nodeinfo.target_tool_path)

    # Create launcher
    bash_launcher = ctx.actions.declare_file("%s.sh" % ctx.label.name)
    ctx.actions.expand_template(
        template = ctx.file._launcher_template,
        output = bash_launcher,
        substitutions = {
            "{{entry_point_path}}": ctx.file.entry_point.short_path,
            "{{initialize_runfiles}}": BASH_INITIALIZE_RUNFILES,
            "{{node}}": node_path,
            "{{workspace_name}}": ctx.workspace_name,
        },
        is_executable = True,
    )

    launcher = create_windows_native_launcher_script(ctx, bash_launcher) if is_windows else bash_launcher

    runfiles = [ctx.file.entry_point]

    if hasattr(nodeinfo, "node"):
        if nodeinfo.node:
            runfiles.append(nodeinfo.node)
    else:
        # TODO(3.0): drop support for deprecated toolchain attributes
        runfiles.extend(nodeinfo.tool_files)

    return DefaultInfo(
        executable = launcher,
        runfiles = ctx.runfiles(files = runfiles),
    )

coverage_merger = rule(
    implementation = _coverage_merger_impl,
    attrs = _ATTRS,
    executable = True,
    toolchains = [
        "@bazel_tools//tools/sh:toolchain_type",
        "@rules_nodejs//nodejs:toolchain_type",
    ],
)
