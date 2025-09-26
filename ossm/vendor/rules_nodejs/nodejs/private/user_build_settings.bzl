"""Function for user args to be set on the command line
"""

load("//nodejs/private/providers:user_build_settings.bzl", "UserBuildSettingInfo")

def _impl(ctx):
    # keeping this as a function to make it easier to
    # check flags or add logic in the future

    # Returns a provider like a normal rule
    return UserBuildSettingInfo(value = ctx.build_setting_value)

user_args = rule(
    implementation = _impl,
    # This line separates a build setting from a regular target, by using
    # the `build_setting` attribute, you mark this rule as a build setting
    # including what raw type it is and if it can be used on the command
    # line or not (if yes, you must set `flag = True`)
    build_setting = config.string(flag = True),
)
