"""env_marker_setting tests."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", "TestingAspectInfo")
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:env_marker_info.bzl", "EnvMarkerInfo")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:env_marker_setting.bzl", "env_marker_setting")  # buildifier: disable=bzl-visibility

def _custom_env_markers_impl(ctx):
    _ = ctx  # @unused
    return [EnvMarkerInfo(env = {
        "os_name": "testos",
    })]

_custom_env_markers = rule(
    implementation = _custom_env_markers_impl,
)

_tests = []

def _test_custom_env_markers(name):
    def _impl(env, target):
        env.expect.where(
            expression = target[TestingAspectInfo].attrs.expression,
        ).that_str(
            target[config_common.FeatureFlagInfo].value,
        ).equals("TRUE")

    env_marker_setting(
        name = name + "_subject",
        expression = "os_name == 'testos'",
    )
    _custom_env_markers(name = name + "_env")
    analysis_test(
        name = name,
        impl = _impl,
        target = name + "_subject",
        config_settings = {
            labels.PIP_ENV_MARKER_CONFIG: str(Label(name + "_env")),
        },
    )

_tests.append(_test_custom_env_markers)

def _test_expr(name):
    def impl(env, target):
        env.expect.where(
            expression = target[TestingAspectInfo].attrs.expression,
        ).that_str(
            target[config_common.FeatureFlagInfo].value,
        ).equals(
            env.ctx.attr.expected,
        )

    cases = {
        "python_full_version_lt_negative": {
            "config_settings": {
                labels.PYTHON_VERSION: "3.12.0",
            },
            "expected": "FALSE",
            "expression": "python_full_version < '3.8'",
        },
        "python_version_gte": {
            "config_settings": {
                labels.PYTHON_VERSION: "3.12.0",
            },
            "expected": "TRUE",
            "expression": "python_version >= '3.12.0'",
        },
    }

    tests = []
    for case_name, case in cases.items():
        test_name = name + "_" + case_name
        tests.append(test_name)
        env_marker_setting(
            name = test_name + "_subject",
            expression = case["expression"],
        )
        analysis_test(
            name = test_name,
            impl = impl,
            target = test_name + "_subject",
            config_settings = case["config_settings"],
            attr_values = {
                "expected": case["expected"],
            },
            attrs = {
                "expected": attr.string(),
            },
        )
    native.test_suite(
        name = name,
        tests = tests,
    )

_tests.append(_test_expr)

def env_marker_setting_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
