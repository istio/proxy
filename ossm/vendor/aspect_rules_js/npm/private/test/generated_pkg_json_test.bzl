"""Unit tests for generated package_json macros
See https://docs.bazel.build/versions/main/skylark/testing.html#for-testing-starlark-utilities
"""

load(":snapshots/wksp/package_json.bzl", rollup_bin = "bin")
load("@bazel_skylib//lib:unittest.bzl", "loadingtest")
load("@bazel_skylib//lib:new_sets.bzl", "sets")

_TEST_TARGET_PREFIX = "__rollup"

def _get_functions_under_test_and_their_args():
    return {
        "rollup": {
            "name": _TEST_TARGET_PREFIX + "-target",
            "outs": ["foo.js"],
            "srcs": [":empty.js"],
            "chdir": native.package_name(),
            "args": ["empty.js", "--format", "cjs", "--file", "foo.js"],
        },
        "rollup_test": {
            "name": _TEST_TARGET_PREFIX + "-test",
        },
        "rollup_binary": {
            "name": _TEST_TARGET_PREFIX + "-binary",
        },
    }

def _get_properties_under_test():
    return {
        "rollup_path": "dist/bin/rollup",
    }

def _target_names_used_for_test():
    return [kwargs["name"] for _, kwargs in _get_functions_under_test_and_their_args().items()]

def test_only_expected_bin_struct_methods(env, bin_struct):
    """Ensure that no new methods have been added to the exported bin method.

    If new methods were added or removed, the above struct defining how to
    construct a target using each method will need to be updated to add
    a new example usecase or removed.

    Args:
        env: the environment to test
        bin_struct: the expected bin entries
    """

    relevant_methods = [m for m in sorted(dir(bin_struct)) if m.startswith("rollup") and not m.endswith("_path")]
    relavent_properties = {
        k: getattr(bin_struct, k)
        for k in sorted(dir(bin_struct))
        if k.endswith("_path")
    }

    # If new generated methods are added to the package_json.bzl file, then the tests here will need
    # to be updated to call those macros.
    loadingtest.equals(env, "only_expected_methods", sorted(_get_functions_under_test_and_their_args().keys()), sorted(relevant_methods))
    loadingtest.equals(env, "only_expected_properties", _get_properties_under_test(), relavent_properties)

# buildifier: disable=function-docstring-args
def test_intermediate_targets_tagged_manual(env):
    """Ensure all targets besides the final output target are tagged manual"""

    existing_rules = native.existing_rules()
    relevant_targets = [t for t in existing_rules.keys() if t.startswith(_TEST_TARGET_PREFIX)]

    expected_not_manual = _target_names_used_for_test()
    generated_targets = sets.to_list(sets.difference(
        sets.make(relevant_targets),
        sets.make(expected_not_manual),
    ))

    for generated_target in generated_targets:
        tagged_manual = "manual" in existing_rules[generated_target]["tags"]
        loadingtest.equals(env, generated_target + "_tagged_manual", tagged_manual, True)

# buildifier: disable=function-docstring
def generated_pkg_json_test(name):
    # Call each of our methods to construct targets as needed by the tests
    for function_name, kwargs in _get_functions_under_test_and_their_args().items():
        function = getattr(rollup_bin, function_name)
        if function == None:
            fail("{} does not exist in exported bin struct".format(function_name))

        function(**kwargs)

    env = loadingtest.make(name)

    test_only_expected_bin_struct_methods(env, rollup_bin)
    test_intermediate_targets_tagged_manual(env)
