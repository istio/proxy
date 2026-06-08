"""Flag to tell if exec or target mode is active."""

load(":py_internal.bzl", "py_internal")

def _bazel_config_mode_impl(ctx):
    return [config_common.FeatureFlagInfo(
        value = "exec" if py_internal.is_tool_configuration(ctx) else "target",
    )]

bazel_config_mode = rule(
    implementation = _bazel_config_mode_impl,
)
