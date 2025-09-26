"""Utility functions for platforms"""

load("@local_config_platform//:constraints.bzl", "HOST_CONSTRAINTS")

def _host_platform_is_darwin():
    return "@platforms//os:osx" in HOST_CONSTRAINTS

def _host_platform_is_linux():
    return "@platforms//os:linux" in HOST_CONSTRAINTS

def _host_platform_is_windows():
    return "@platforms//os:windows" in HOST_CONSTRAINTS

platform_utils = struct(
    host_platform_is_darwin = _host_platform_is_darwin,
    host_platform_is_linux = _host_platform_is_linux,
    host_platform_is_windows = _host_platform_is_windows,
)
