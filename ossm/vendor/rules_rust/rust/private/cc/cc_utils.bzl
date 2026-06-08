"""Rust CC utilities"""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def _cc_empty_library_impl(_ctx):
    return [CcInfo()]

cc_empty_library = rule(
    doc = "A rule that provides an empty `CcInfo`.",
    implementation = _cc_empty_library_impl,
    provides = [CcInfo],
)
