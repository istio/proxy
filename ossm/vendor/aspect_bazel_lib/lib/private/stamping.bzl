"""A small utility module dedicated to detecting whether or not the `--stamp` flag is enabled
This module can be removed likely after the following PRs ar addressed:
- https://github.com/bazelbuild/bazel/issues/11164
"""

StampSettingInfo = provider(
    doc = "Information about the `--stamp` command line flag",
    fields = {
        "value": "bool: Whether or not the `--stamp` flag was enabled",
    },
)

def _stamp_build_setting_impl(ctx):
    return StampSettingInfo(value = ctx.attr.value)

_stamp_build_setting = rule(
    doc = "Adapter from our config_setting to a Provider for downstream rules",
    implementation = _stamp_build_setting_impl,
    attrs = {
        "value": attr.bool(
            doc = "The default value of the stamp build flag",
            mandatory = True,
        ),
    },
)

def stamp_build_setting(name, visibility = ["//visibility:public"]):
    native.config_setting(
        name = "stamp_detect",
        values = {"stamp": "1"},
        visibility = visibility,
    )

    _stamp_build_setting(
        name = name,
        value = select({
            ":stamp_detect": True,
            "//conditions:default": False,
        }),
        visibility = visibility,
    )

def is_stamping_enabled(attr):
    """Determine whether or not build stamping is enabled.

    Args:
        attr (struct): A rule's struct of attributes (`ctx.attr`)
    Returns:
        bool: The stamp value
    """
    stamp_num = getattr(attr, "stamp", -1)
    if stamp_num > 0:
        return True
    elif stamp_num == 0:
        return False
    elif stamp_num < 0:
        stamp_flag = getattr(attr, "_stamp_flag", None)
        return stamp_flag[StampSettingInfo].value if stamp_flag else False
    else:
        fail("Unexpected `stamp` value: {}".format(stamp_num))
