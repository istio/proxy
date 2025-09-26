"""Provider for implementing environment marker values."""

EnvMarkerInfo = provider(
    doc = """
The values to use during environment marker evaluation.

:::{seealso}
The {obj}`--//python/config_settings:pip_env_marker_config` flag.
:::

:::{versionadded} 1.5.0
""",
    fields = {
        "env": """
:type: dict[str, str]

The values to use for environment markers when evaluating an expression.

The keys and values should be compatible with the [PyPA dependency specifiers
specification](https://packaging.python.org/en/latest/specifications/dependency-specifiers/).

Missing values will be set to the specification's defaults or computed using
available toolchain information.
""",
    },
)
