"""Unittest to verify proc-macro targets"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//rust:defs.bzl", "rust_proc_macro", "rust_test")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
    "assert_list_contains_adjacent_elements",
    "assert_list_contains_adjacent_elements_not",
)

def _proc_macro_test_targets(edition):
    """Define a set of `rust_proc_macro` targets for testing

    Args:
        edition (str): The rust edition to use for the new targets
    """
    rust_proc_macro(
        name = "proc_macro_{}".format(edition),
        srcs = [
            "proc_macro_{}.rs".format(edition),
        ],
        edition = edition,
        visibility = ["//test:__subpackages__"],
    )

    rust_test(
        name = "wrapper_rule_for_macro_{}".format(edition),
        crate = ":proc_macro_{}".format(edition),
        edition = edition,
    )

def _unit_test_impl(ctx, edition, is_wrapper):
    env = analysistest.begin(ctx)
    actions = analysistest.target_under_test(env).actions
    action = actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    if edition == "2015":
        # Edition 2015 does not use `--extern proc_macro` instead this
        # must be explicitly set in Rust code.
        assert_list_contains_adjacent_elements_not(env, action.argv, ["--extern", "proc_macro"])
    elif edition == "2018":
        # `--extern proc_macro` is required to resolve build proc-macro
        assert_list_contains_adjacent_elements(env, action.argv, ["--extern", "proc_macro"])
        if not is_wrapper:
            assert_argv_contains(env, action, "--edition=2018")
    else:
        fail("Unexpected edition")

    return analysistest.end(env)

def _extern_flag_not_passed_when_compiling_macro_2015_impl(ctx):
    return _unit_test_impl(ctx, "2015", False)

def _extern_flag_passed_when_compiling_macro_2018_impl(ctx):
    return _unit_test_impl(ctx, "2018", False)

def _extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015_impl(ctx):
    return _unit_test_impl(ctx, "2015", True)

def _extern_flag_passed_when_compiling_macro_wrapper_rule_2018_impl(ctx):
    return _unit_test_impl(ctx, "2018", True)

extern_flag_not_passed_when_compiling_macro_2015_test = analysistest.make(_extern_flag_not_passed_when_compiling_macro_2015_impl)
extern_flag_passed_when_compiling_macro_2018_test = analysistest.make(_extern_flag_passed_when_compiling_macro_2018_impl)
extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015_test = analysistest.make(_extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015_impl)
extern_flag_passed_when_compiling_macro_wrapper_rule_2018_test = analysistest.make(_extern_flag_passed_when_compiling_macro_wrapper_rule_2018_impl)

def _proc_macro_test():
    """Generate targets and tests"""

    _proc_macro_test_targets("2015")
    _proc_macro_test_targets("2018")

    extern_flag_not_passed_when_compiling_macro_2015_test(
        name = "extern_flag_not_passed_when_compiling_macro_2015",
        target_under_test = ":proc_macro_2015",
    )
    extern_flag_passed_when_compiling_macro_2018_test(
        name = "extern_flag_passed_when_compiling_macro_2018",
        target_under_test = ":proc_macro_2018",
    )
    extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015_test(
        name = "extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015",
        target_under_test = ":wrapper_rule_for_macro_2015",
    )
    extern_flag_passed_when_compiling_macro_wrapper_rule_2018_test(
        name = "extern_flag_passed_when_compiling_macro_wrapper_rule_2018",
        target_under_test = ":wrapper_rule_for_macro_2018",
    )

def proc_macro_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _proc_macro_test()

    native.test_suite(
        name = name,
        tests = [
            ":extern_flag_not_passed_when_compiling_macro_2015",
            ":extern_flag_passed_when_compiling_macro_2018",
            ":extern_flag_not_passed_when_compiling_macro_wrapper_rule_2015",
            ":extern_flag_passed_when_compiling_macro_wrapper_rule_2018",
        ],
    )
