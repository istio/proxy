"""Alias-like rule for testing."""

load("@rules_rust//rust:defs.bzl", "rust_common")

def _custom_alias_impl(ctx):
    actual = ctx.attr.actual
    return [actual[rust_common.crate_info], actual[rust_common.dep_info]]

custom_alias = rule(
    implementation = _custom_alias_impl,
    attrs = {
        "actual": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
    },
)
