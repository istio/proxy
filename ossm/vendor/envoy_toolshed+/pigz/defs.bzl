"""Public pigz toolchain surface."""

load(
    "//pigz:toolchain.bzl",
    _PigzInfo = "PigzInfo",
    _pigz_toolchain = "pigz_toolchain",
    _resolved_pigz = "resolved_pigz",
)

PIGZ_TOOLCHAIN_TYPE = str(Label("@envoy_toolshed//pigz:toolchain_type"))
PigzInfo = _PigzInfo
pigz_toolchain = _pigz_toolchain
resolved_pigz = _resolved_pigz
