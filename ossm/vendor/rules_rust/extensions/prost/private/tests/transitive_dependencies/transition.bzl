"""Transition rule for the transitive dependencies test case."""

load("@rules_rust//rust:defs.bzl", "rust_common")

def _extra_toolchain_transition_impl(settings, attr):
    return {
        "//command_line_option:extra_toolchains": [
            attr.toolchain,
        ] + settings["//command_line_option:extra_toolchains"],
    }

_extra_toolchain_transition = transition(
    implementation = _extra_toolchain_transition_impl,
    inputs = ["//command_line_option:extra_toolchains"],
    outputs = ["//command_line_option:extra_toolchains"],
)

def _extra_toolchain_wrapper_impl(ctx):
    return [
        ctx.attr.dep[DefaultInfo],
        ctx.attr.dep[rust_common.crate_group_info],
    ]

extra_toolchain_wrapper = rule(
    implementation = _extra_toolchain_wrapper_impl,
    doc = "Transition rule used for unit testing the prost toolchains.",
    attrs = {
        "dep": attr.label(),
        "toolchain": attr.label(),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    cfg = _extra_toolchain_transition,
)
