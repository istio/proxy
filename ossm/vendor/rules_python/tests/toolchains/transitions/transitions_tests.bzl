# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""

load("@pythons_hub//:versions.bzl", "DEFAULT_PYTHON_VERSION", "MINOR_MAPPING")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:versions.bzl", "TOOL_VERSIONS")
load("//python/private:bzlmod_enabled.bzl", "BZLMOD_ENABLED")  # buildifier: disable=bzl-visibility
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private:full_version.bzl", "full_version")  # buildifier: disable=bzl-visibility
load("//python/private:toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility

_analysis_tests = []

def _transition_impl(input_settings, attr):
    """Transition based on python_version flag.

    This is a simple transition impl that a user of rules_python may implement
    for their own rule.
    """
    settings = {
        labels.PYTHON_VERSION: input_settings[labels.PYTHON_VERSION],
    }
    if attr.python_version:
        settings[labels.PYTHON_VERSION] = attr.python_version
    return settings

_python_version_transition = transition(
    implementation = _transition_impl,
    inputs = [labels.PYTHON_VERSION],
    outputs = [labels.PYTHON_VERSION],
)

TestInfo = provider(
    doc = "A simple test provider to forward the values for the assertion.",
    fields = {"got": "", "want": ""},
)

def _impl(ctx):
    if ctx.attr.skip:
        return [TestInfo(got = "", want = "")]

    exec_tools = ctx.toolchains[EXEC_TOOLS_TOOLCHAIN_TYPE].exec_tools
    got_version = exec_tools.exec_interpreter[platform_common.ToolchainInfo].py3_runtime.interpreter_version_info
    got = "{}.{}.{}".format(
        got_version.major,
        got_version.minor,
        got_version.micro,
    )
    if got_version.releaselevel != "final":
        got = "{}{}{}".format(
            got,
            "rc" if got_version.releaselevel == "candidate" else got_version.releaselevel[0],
            got_version.serial,
        )

    return [
        TestInfo(
            got = got,
            want = ctx.attr.want_version,
        ),
    ]

_simple_transition = rule(
    implementation = _impl,
    attrs = {
        "python_version": attr.string(
            doc = "The input python version which we transition on.",
        ),
        "skip": attr.bool(
            doc = "Whether to skip the test",
        ),
        "want_version": attr.string(
            doc = "The python version that we actually expect to receive.",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    toolchains = [
        config_common.toolchain_type(
            EXEC_TOOLS_TOOLCHAIN_TYPE,
            mandatory = False,
        ),
    ],
    cfg = _python_version_transition,
)

def _test_transitions(*, name, tests, skip = False):
    """A reusable rule so that we can split the tests."""
    targets = {}
    for test_name, (input_version, want_version) in tests.items():
        target_name = "{}_{}".format(name, test_name)
        targets["python_" + test_name] = target_name
        rt_util.helper_target(
            _simple_transition,
            name = target_name,
            python_version = input_version,
            want_version = want_version,
            skip = skip,
        )

    analysis_test(
        name = name,
        impl = _test_transition_impl,
        targets = targets,
    )

def _test_transition_impl(env, targets):
    # Check that the forwarded version from the PyRuntimeInfo is correct
    for target in dir(targets):
        if not target.startswith("python"):
            # Skip other attributes that might be not the ones we set (e.g. to_json, to_proto).
            continue

        test_info = env.expect.that_target(getattr(targets, target)).provider(
            TestInfo,
            factory = lambda v, meta: v,
        )
        env.expect.that_str(test_info.got).equals(test_info.want)

def _test_full_version(name):
    """Check that python_version transitions work.

    Expectation is to get the same full version that we input.
    """
    _test_transitions(
        name = name,
        tests = {
            v.replace(".", "_"): (v, v)
            for v in TOOL_VERSIONS
        },
    )

_analysis_tests.append(_test_full_version)

def _test_minor_versions(name):
    """Ensure that MINOR_MAPPING versions are correctly selected."""
    _test_transitions(
        name = name,
        skip = not BZLMOD_ENABLED,
        tests = {
            minor.replace(".", "_"): (minor, full)
            for minor, full in MINOR_MAPPING.items()
        },
    )

_analysis_tests.append(_test_minor_versions)

def _test_default(name):
    """Check the default version.

    Lastly, if we don't provide any version to the transition, we should
    get the default version
    """
    default_version = full_version(
        version = DEFAULT_PYTHON_VERSION,
        minor_mapping = MINOR_MAPPING,
    ) if DEFAULT_PYTHON_VERSION else ""

    _test_transitions(
        name = name,
        skip = not BZLMOD_ENABLED,
        tests = {
            "default": (None, default_version),
        },
    )

_analysis_tests.append(_test_default)

def transitions_test_suite(name):
    test_suite(
        name = name,
        tests = _analysis_tests,
    )
