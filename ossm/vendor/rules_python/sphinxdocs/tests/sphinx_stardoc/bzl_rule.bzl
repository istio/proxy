"""Tests for rules."""

load(":bzl_providers.bzl", OtherGenericInfo = "GenericInfo")

# buildifier: disable=provider-params
GenericInfo = provider()

# buildifier: disable=provider-params
P1 = provider()

# buildifier: disable=provider-params
P2 = provider()

def _impl(ctx):
    _ = ctx  # @unused

bzl_rule = rule(
    implementation = _impl,
    attrs = {
        "srcs": attr.label(
            providers = [[GenericInfo], [OtherGenericInfo], [P1, P2], [platform_common.ToolchainInfo]],
        ),
    },
)
