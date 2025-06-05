"""This file contains definitions of all current incompatible flags.

See COMPATIBILITY.md for the backwards compatibility policy.
"""

IncompatibleFlagInfo = provider(
    doc = "Provider for the current value of an incompatible flag.",
    fields = {
        "enabled": "(bool) whether the flag is enabled",
        "issue": "(string) link to the github issue associated with this flag",
    },
)

def _incompatible_flag_impl(ctx):
    return [IncompatibleFlagInfo(enabled = ctx.build_setting_value, issue = ctx.attr.issue)]

incompatible_flag = rule(
    doc = "A rule defining an incompatible flag.",
    implementation = _incompatible_flag_impl,
    build_setting = config.bool(flag = True),
    attrs = {
        "issue": attr.string(
            doc = "The link to the github issue associated with this flag",
            mandatory = True,
        ),
    },
)
