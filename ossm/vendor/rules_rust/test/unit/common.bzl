"""Common test helpers for unit tests."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")

def assert_argv_contains_not(env, action, flag):
    asserts.true(
        env,
        flag not in action.argv,
        "Expected {args} to not contain {flag}".format(args = action.argv, flag = flag),
    )

def assert_argv_contains(env, action, flag):
    asserts.true(
        env,
        flag in action.argv,
        "Expected {args} to contain {flag}".format(args = action.argv, flag = flag),
    )

def assert_argv_contains_prefix_suffix(env, action, prefix, suffix):
    for found_flag in action.argv:
        if found_flag.startswith(prefix) and found_flag.endswith(suffix):
            return
    unittest.fail(
        env,
        "Expected an arg with prefix '{prefix}' and suffix '{suffix}' in {args}".format(
            prefix = prefix,
            suffix = suffix,
            args = action.argv,
        ),
    )

def assert_argv_contains_prefix(env, action, prefix):
    for found_flag in action.argv:
        if found_flag.startswith(prefix):
            return
    unittest.fail(
        env,
        "Expected an arg with prefix '{prefix}' in {args}".format(
            prefix = prefix,
            args = action.argv,
        ),
    )

def assert_argv_contains_prefix_not(env, action, prefix):
    for found_flag in action.argv:
        if found_flag.startswith(prefix):
            unittest.fail(
                env,
                "Expected an arg with prefix '{prefix}' to not appear in {args}".format(
                    prefix = prefix,
                    args = action.argv,
                ),
            )

def assert_action_mnemonic(env, action, mnemonic):
    if not action.mnemonic == mnemonic:
        unittest.fail(
            env,
            "Expected the action to have the mnemonic '{expected}', but got '{actual}'".format(
                expected = mnemonic,
                actual = action.mnemonic,
            ),
        )

def _startswith(list, prefix):
    if len(list) < len(prefix):
        return False
    for pair in zip(list[:len(prefix) + 1], prefix):
        if pair[0] != pair[1]:
            return False
    return True

def assert_list_contains_adjacent_elements(env, list_under_test, adjacent_elements):
    """Assert that list_under_test contains given adjacent flags.

    Args:
          env: env from analysistest.begin(ctx).
          list_under_test: list supposed to contain adjacent elements.
          adjacent_elements: list of elements to be found inside list_under_test.
    """
    for idx in range(len(list_under_test)):
        if list_under_test[idx] == adjacent_elements[0]:
            if _startswith(list_under_test[idx:], adjacent_elements):
                return

    unittest.fail(
        env,
        "Expected the to find '{expected}' within '{actual}'".format(
            expected = adjacent_elements,
            actual = list_under_test,
        ),
    )

def assert_list_contains_adjacent_elements_not(env, list_under_test, adjacent_elements):
    """Assert that list_under_test does not contains given adjacent flags.

    Args:
          env: env from analysistest.begin(ctx).
          list_under_test: list supposed not to contain adjacent elements.
          adjacent_elements: list of elements not to be found inside list_under_test."""
    for idx in range(len(list_under_test)):
        if list_under_test[idx] == adjacent_elements[0]:
            if _startswith(list_under_test[idx:], adjacent_elements):
                unittest.fail(
                    env,
                    "Expected not the to find '{expected}' within '{actual}'".format(
                        expected = adjacent_elements,
                        actual = list_under_test,
                    ),
                )

def assert_env_value(env, action, key, value):
    asserts.true(
        env,
        action.env[key] == value,
        "Expected env[{key}] to be equal to '{value}', got '{real_value}'".format(
            key = key,
            value = value,
            real_value = action.env[key],
        ),
    )
