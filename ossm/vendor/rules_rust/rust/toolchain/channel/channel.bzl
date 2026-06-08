"""Rules for representing Rust toolchain channels"""

_CHANNELS = [
    "beta",
    "nightly",
    "stable",
]

RustToolchainChannelInfo = provider(
    doc = "A provider describing the Rust toolchain channel.",
    fields = {
        "value": "string: Can be {}".format(_CHANNELS),
    },
)

def _rust_toolchain_channel_flag_impl(ctx):
    value = ctx.build_setting_value
    if value not in _CHANNELS:
        fail(str(ctx.label) + " build setting allowed to take values {" +
             ", ".join(_CHANNELS) + "} but was set to unallowed value " +
             value)
    return RustToolchainChannelInfo(value = value)

rust_toolchain_channel_flag = rule(
    doc = "A build setting which represents the Rust toolchain channel. The allowed values are {}".format(_CHANNELS),
    implementation = _rust_toolchain_channel_flag_impl,
    build_setting = config.string(
        flag = True,
    ),
)
