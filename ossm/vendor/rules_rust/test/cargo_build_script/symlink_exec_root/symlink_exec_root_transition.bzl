"""A transition for `@rules_rust//cargo/settings:experimental_symlink_execroot`"""

load("//rust:rust_common.bzl", "BuildInfo")

def _symlink_execroot_setting_transition_impl(_attr, _settings):
    return {
        "//cargo/settings:experimental_symlink_execroot": True,
    }

symlink_execroot_setting_transition = transition(
    implementation = _symlink_execroot_setting_transition_impl,
    inputs = [],
    outputs = ["//cargo/settings:experimental_symlink_execroot"],
)

def _symlink_execroot_cargo_build_script(ctx):
    script = ctx.attr.script

    return [
        script[BuildInfo],
        script[OutputGroupInfo],
    ]

symlink_execroot_cargo_build_script = rule(
    implementation = _symlink_execroot_cargo_build_script,
    doc = "A wrapper for cargo_build_script which transitions `experimental_symlink_execroot`",
    attrs = {
        "script": attr.label(
            doc = "A `cargo_build_script` target.",
            mandatory = True,
            providers = [BuildInfo],
        ),
        "_allowlist_function_transition": attr.label(
            default = Label("//tools/allowlists/function_transition_allowlist"),
        ),
    },
    cfg = symlink_execroot_setting_transition,
)
