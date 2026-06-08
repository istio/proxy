"""Implement a flag for matching the dependency specifiers at analysis time."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//python/private:common_labels.bzl", "labels")
load("//python/private:toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")
load(":env_marker_info.bzl", "EnvMarkerInfo")
load(":pep508_env.bzl", "create_env", "set_missing_env_defaults")
load(":pep508_evaluate.bzl", "evaluate")

# Use capitals to hint its not an actual boolean type.
_ENV_MARKER_TRUE = "TRUE"
_ENV_MARKER_FALSE = "FALSE"

def env_marker_setting(*, name, expression, **kwargs):
    """Creates an env_marker setting.

    Generated targets:

    * `is_{name}_true`: config_setting that matches when the expression is true.
    * `{name}`: env marker target that evalutes the expression.

    Args:
        name: {type}`str` target name
        expression: {type}`str` the environment marker string to evaluate
        **kwargs: {type}`dict` additional common kwargs.
    """
    native.config_setting(
        name = "is_{}_true".format(name),
        flag_values = {
            ":{}".format(name): _ENV_MARKER_TRUE,
        },
        **kwargs
    )
    _env_marker_setting(
        name = name,
        expression = expression,
        **kwargs
    )

def _env_marker_setting_impl(ctx):
    env = create_env()
    env.update(
        ctx.attr._env_marker_config_flag[EnvMarkerInfo].env,
    )

    runtime = ctx.toolchains[TARGET_TOOLCHAIN_TYPE].py3_runtime

    if "python_version" not in env:
        if runtime.interpreter_version_info:
            version_info = runtime.interpreter_version_info
            env["python_version"] = "{major}.{minor}".format(
                major = version_info.major,
                minor = version_info.minor,
            )
            full_version = _format_full_version(version_info)
            env["python_full_version"] = full_version
            env["implementation_version"] = full_version
        else:
            env["python_version"] = _get_flag(ctx.attr._python_version_major_minor_flag)
            full_version = _get_flag(ctx.attr._python_full_version_flag)
            env["python_full_version"] = full_version
            env["implementation_version"] = full_version

    if "implementation_name" not in env and runtime.implementation_name:
        env["implementation_name"] = runtime.implementation_name

    set_missing_env_defaults(env)
    if evaluate(ctx.attr.expression, env = env):
        value = _ENV_MARKER_TRUE
    else:
        value = _ENV_MARKER_FALSE
    return [config_common.FeatureFlagInfo(value = value)]

_env_marker_setting = rule(
    doc = """
Evaluates an environment marker expression using target configuration info.

See
https://packaging.python.org/en/latest/specifications/dependency-specifiers
for the specification of behavior.
""",
    implementation = _env_marker_setting_impl,
    attrs = {
        "expression": attr.string(
            mandatory = True,
            doc = "Environment marker expression to evaluate.",
        ),
        "_env_marker_config_flag": attr.label(
            default = labels.PIP_ENV_MARKER_CONFIG,
            providers = [EnvMarkerInfo],
        ),
        "_python_full_version_flag": attr.label(
            default = labels.PYTHON_VERSION,
            providers = [config_common.FeatureFlagInfo],
        ),
        "_python_version_major_minor_flag": attr.label(
            default = labels.PYTHON_VERSION_MAJOR_MINOR,
            providers = [config_common.FeatureFlagInfo],
        ),
    },
    provides = [config_common.FeatureFlagInfo],
    toolchains = [
        TARGET_TOOLCHAIN_TYPE,
    ],
)

def _format_full_version(info):
    """Format the full python interpreter version.

    Adapted from spec code at:
    https://packaging.python.org/en/latest/specifications/dependency-specifiers/#environment-markers

    Args:
        info: The provider from the Python runtime.

    Returns:
        a {type}`str` with the version
    """
    kind = info.releaselevel
    if kind == "final":
        kind = ""
        serial = ""
    else:
        kind = kind[0] if kind else ""
        serial = str(info.serial) if info.serial else ""

    return "{major}.{minor}.{micro}{kind}{serial}".format(
        v = info,
        major = info.major,
        minor = info.minor,
        micro = info.micro,
        kind = kind,
        serial = serial,
    )

def _get_flag(t):
    if config_common.FeatureFlagInfo in t:
        return t[config_common.FeatureFlagInfo].value
    if BuildSettingInfo in t:
        return t[BuildSettingInfo].value
    fail("Should not occur: {} does not have necessary providers")
