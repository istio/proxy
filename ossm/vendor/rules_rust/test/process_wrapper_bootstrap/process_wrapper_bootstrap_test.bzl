"""Starlark unit tests for the bootstrap process wrapper"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//test/unit:common.bzl", "assert_action_mnemonic")

def _enable_sh_toolchain_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    if ctx.attr.expected_ext == ".bat":
        assert_action_mnemonic(env, target.actions[0], "ExecutableSymlink")
    else:
        assert_action_mnemonic(env, target.actions[0], "TemplateExpand")

    return analysistest.end(env)

_enable_sh_toolchain_test = analysistest.make(
    _enable_sh_toolchain_test_impl,
    config_settings = {
        str(Label("//rust/settings:experimental_use_sh_toolchain_for_bootstrap_process_wrapper")): True,
    },
    attrs = {
        "expected_ext": attr.string(
            doc = "The expected extension for the bootstrap script.",
            mandatory = True,
            values = [
                ".bat",
                ".sh",
            ],
        ),
    },
)

def _disable_sh_toolchain_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    assert_action_mnemonic(env, target.actions[0], "ExecutableSymlink")

    return analysistest.end(env)

_disable_sh_toolchain_test = analysistest.make(
    _disable_sh_toolchain_test_impl,
    config_settings = {
        str(Label("//rust/settings:experimental_use_sh_toolchain_for_bootstrap_process_wrapper")): False,
    },
)

def process_wrapper_bootstrap_test_suite(name, **kwargs):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
        **kwargs (dict): Additional keyword arguments.
    """

    _enable_sh_toolchain_test(
        name = "enable_sh_toolchain_test",
        target_under_test = Label("//util/process_wrapper:bootstrap_process_wrapper"),
        expected_ext = select({
            "@platforms//os:windows": ".bat",
            "//conditions:default": ".sh",
        }),
    )

    _disable_sh_toolchain_test(
        name = "disable_sh_toolchain_test",
        target_under_test = Label("//util/process_wrapper:bootstrap_process_wrapper"),
    )

    native.test_suite(
        name = name,
        tests = [
            ":disable_sh_toolchain_test",
            ":enable_sh_toolchain_test",
        ],
        **kwargs
    )
