"""Unittest to verify workspace status stamping is applied to environment files"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//rust:defs.bzl", "rust_binary", "rust_common", "rust_library", "rust_test")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
    "assert_argv_contains_not",
)

_STAMP_ATTR_VALUES = (0, 1, -1)
_BUILD_FLAG_VALUES = ("true", "false")

def _assert_stamped(env, action):
    assert_argv_contains(env, action, "--volatile-status-file")
    assert_argv_contains(env, action, "bazel-out/volatile-status.txt")

    assert_argv_contains(env, action, "--stable-status-file")
    assert_argv_contains(env, action, "bazel-out/stable-status.txt")

def _assert_not_stamped(env, action):
    assert_argv_contains_not(env, action, "--volatile-status-file")
    assert_argv_contains_not(env, action, "bazel-out/volatile-status.txt")

    assert_argv_contains_not(env, action, "--stable-status-file")
    assert_argv_contains_not(env, action, "bazel-out/stable-status.txt")

def _stamp_build_flag_test_impl(ctx, flag_value):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    is_test = target[rust_common.crate_info].is_test
    is_bin = target[rust_common.crate_info].type == "bin"

    # bazel build --stamp should lead to stamped rust binaries, but not
    # libraries and tests.
    if flag_value:
        if is_bin and not is_test:
            _assert_stamped(env, action)
        else:
            _assert_not_stamped(env, action)
    else:
        _assert_not_stamped(env, action)

    return analysistest.end(env)

def _stamp_build_flag_is_true_impl(ctx):
    return _stamp_build_flag_test_impl(ctx, True)

def _stamp_build_flag_is_false_impl(ctx):
    return _stamp_build_flag_test_impl(ctx, False)

stamp_build_flag_is_true_test = analysistest.make(
    _stamp_build_flag_is_true_impl,
    config_settings = {
        "//command_line_option:stamp": True,
    },
)

stamp_build_flag_is_false_test = analysistest.make(
    _stamp_build_flag_is_false_impl,
    config_settings = {
        "//command_line_option:stamp": False,
    },
)

def _build_flag_tests():
    tests = []
    for stamp_value in _BUILD_FLAG_VALUES:
        if stamp_value == "true":
            name = "default_with_build_flag_on"
            features = ["always_stamp"]
            build_flag_stamp_test = stamp_build_flag_is_true_test
        else:
            name = "default_with_build_flag_off"
            features = ["never_stamp"]
            build_flag_stamp_test = stamp_build_flag_is_false_test

        rust_library(
            name = "{}_lib".format(name),
            srcs = ["stamp.rs"],
            rustc_env_files = ["stamp.env"],
            edition = "2018",
            # Building with --stamp should not affect rust libraries
            crate_features = ["never_stamp"],
        )

        rust_binary(
            name = "{}_bin".format(name),
            srcs = ["stamp_main.rs"],
            edition = "2018",
            deps = ["{}_lib".format(name)],
            rustc_env_files = ["stamp.env"],
            crate_features = features,
        )

        rust_test(
            name = "{}_test".format(name),
            crate = "{}_lib".format(name),
            edition = "2018",
            rustc_env_files = ["stamp.env"],
            # Building with --stamp should not affect tests
            crate_features = ["never_stamp"],
        )

        build_flag_stamp_test(
            name = "lib_{}_test".format(name),
            target_under_test = "{}_lib".format(name),
        )
        build_flag_stamp_test(
            name = "bin_{}_test".format(name),
            target_under_test = "{}_bin".format(name),
        )

        build_flag_stamp_test(
            name = "test_{}_test".format(name),
            target_under_test = "{}_test".format(name),
        )

        tests.extend([
            "lib_{}_test".format(name),
            "bin_{}_test".format(name),
            "test_{}_test".format(name),
        ])
    return tests

def _attribute_stamp_test_impl(ctx, attribute_value, build_flag_value):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    if attribute_value == 1:
        _assert_stamped(env, action)
    elif attribute_value == 0:
        _assert_not_stamped(env, action)
    elif build_flag_value:
        _assert_stamped(env, action)
    else:
        _assert_not_stamped(env, action)

    return analysistest.end(env)

def _always_stamp_build_flag_is_true_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = 1, build_flag_value = True)

def _always_stamp_build_flag_is_false_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = 1, build_flag_value = False)

def _never_stamp_build_flag_is_true_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = 0, build_flag_value = True)

def _never_stamp_build_flag_is_false_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = 0, build_flag_value = False)

def _consult_build_flag_value_is_true_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = -1, build_flag_value = True)

def _consult_build_flag_value_is_false_test_impl(ctx):
    return _attribute_stamp_test_impl(ctx, attribute_value = -1, build_flag_value = False)

always_stamp_test_build_flag_is_true_test = analysistest.make(
    _always_stamp_build_flag_is_true_test_impl,
    config_settings = {
        "//command_line_option:stamp": True,
    },
)

always_stamp_test_build_flag_is_false_test = analysistest.make(
    _always_stamp_build_flag_is_false_test_impl,
    config_settings = {
        "//command_line_option:stamp": False,
    },
)

never_stamp_test_build_flag_is_true_test = analysistest.make(
    _never_stamp_build_flag_is_true_test_impl,
    config_settings = {
        "//command_line_option:stamp": True,
    },
)

never_stamp_test_build_flag_is_false_test = analysistest.make(
    _never_stamp_build_flag_is_false_test_impl,
    config_settings = {
        "//command_line_option:stamp": False,
    },
)

consult_build_flag_value_is_true_test = analysistest.make(
    _consult_build_flag_value_is_true_test_impl,
    config_settings = {
        "//command_line_option:stamp": True,
    },
)

consult_build_flag_value_is_false_test = analysistest.make(
    _consult_build_flag_value_is_false_test_impl,
    config_settings = {
        "//command_line_option:stamp": False,
    },
)

def _stamp_attribute_tests():
    tests = []

    for stamp_value in _STAMP_ATTR_VALUES:
        for flag_value in _BUILD_FLAG_VALUES:
            if stamp_value == 1:
                name = "always_stamp_build_flag_{}".format(flag_value)
                features = ["always_stamp_build_flag_{}".format(flag_value)]
                stamp_attr_test = always_stamp_test_build_flag_is_true_test if flag_value == "true" else always_stamp_test_build_flag_is_false_test
            elif stamp_value == 0:
                name = "never_stamp_build_flag_{}".format(flag_value)
                features = ["never_stamp_build_flag_{}".format(flag_value)]
                stamp_attr_test = never_stamp_test_build_flag_is_true_test if flag_value == "true" else never_stamp_test_build_flag_is_false_test
            else:
                name = "consult_cmdline_value_is_{}".format(flag_value)
                features = ["consult_cmdline_value_is_{}".format(flag_value)]
                stamp_attr_test = consult_build_flag_value_is_true_test if flag_value == "true" else consult_build_flag_value_is_false_test

            rust_library(
                name = "{}_lib".format(name),
                srcs = ["stamp.rs"],
                edition = "2018",
                rustc_env_files = [":stamp.env"],
                stamp = stamp_value,
                crate_features = features,
            )

            rust_test(
                name = "{}_unit_test".format(name),
                crate = ":{}_lib".format(name),
                edition = "2018",
                rustc_env_files = [":stamp.env"],
                stamp = stamp_value,
                crate_features = features,
                # We disable this test so that it doesn't try to run with bazel test //test/...
                # The reason for this is because then it is sensitive to the --stamp value which
                # we override in the unit test implementation.
                tags = ["manual"],
            )

            rust_binary(
                name = "{}_bin".format(name),
                srcs = ["stamp_main.rs"],
                edition = "2018",
                deps = [":{}_lib".format(name)],
                rustc_env_files = [":stamp.env"],
                stamp = stamp_value,
                crate_features = features,
            )

            stamp_attr_test(
                name = "lib_{}_test".format(name),
                target_under_test = "{}_lib".format(name),
            )
            stamp_attr_test(
                name = "bin_{}_test".format(name),
                target_under_test = "{}_bin".format(name),
            )

            stamp_attr_test(
                name = "test_{}_test".format(name),
                target_under_test = "{}_unit_test".format(name),
            )

            tests.extend([
                "lib_{}_test".format(name),
                "bin_{}_test".format(name),
                "test_{}_test".format(name),
            ])
    return tests

def _process_wrapper_with_stamp_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    _assert_not_stamped(env, action)

    return analysistest.end(env)

process_wrapper_with_stamp_test = analysistest.make(
    _process_wrapper_with_stamp_test_impl,
    config_settings = {
        "//command_line_option:stamp": True,
    },
)

def _process_wrapper_tests():
    process_wrapper_with_stamp_test(
        name = "test_process_wrapper_with_stamp_test",
        target_under_test = "//util/process_wrapper:process_wrapper",
    )

    return ["test_process_wrapper_with_stamp_test"]

def stamp_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """
    tests = _build_flag_tests() + _stamp_attribute_tests() + _process_wrapper_tests()

    native.test_suite(
        name = name,
        tests = tests,
    )
