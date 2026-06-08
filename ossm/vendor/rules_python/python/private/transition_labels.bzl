"""Flags that terminal rules should allow transitioning on by default.

Terminal rules are e.g. py_binary, py_test, or packaging rules.
"""

load("@bazel_skylib//lib:collections.bzl", "collections")
load("@rules_python_internal//:extra_transition_settings.bzl", "EXTRA_TRANSITION_SETTINGS")
load(":common_labels.bzl", "labels")

_BASE_TRANSITION_LABELS = [
    labels.ADD_SRCS_TO_RUNFILES,
    labels.BOOTSTRAP_IMPL,
    labels.DEBUGGER,
    labels.EXEC_TOOLS_TOOLCHAIN,
    labels.PIP_ENV_MARKER_CONFIG,
    labels.PIP_WHL_MUSLC_VERSION,
    labels.PIP_WHL,
    labels.PIP_WHL_GLIBC_VERSION,
    labels.PIP_WHL_OSX_ARCH,
    labels.PIP_WHL_OSX_VERSION,
    labels.PRECOMPILE,
    labels.PRECOMPILE_SOURCE_RETENTION,
    labels.PYTHON_SRC,
    labels.PYTHON_VERSION,
    labels.PY_FREETHREADED,
    labels.PY_LINUX_LIBC,
    labels.VENVS_SITE_PACKAGES,
    labels.VENVS_USE_DECLARE_SYMLINK,
]

TRANSITION_LABELS = collections.uniq(
    _BASE_TRANSITION_LABELS + EXTRA_TRANSITION_SETTINGS,
)
