"Public providers, aspects and helpers that are shipped in the built-in rules_nodejs repository."

load(
    "//nodejs/private/providers:stamp_setting_info.bzl",
    _STAMP_ATTR = "STAMP_ATTR",
    _StampSettingInfo = "StampSettingInfo",
)
load(
    "//nodejs/private/providers:user_build_settings.bzl",
    _UserBuildSettingInfo = "UserBuildSettingInfo",
)

UserBuildSettingInfo = _UserBuildSettingInfo
StampSettingInfo = _StampSettingInfo
STAMP_ATTR = _STAMP_ATTR
