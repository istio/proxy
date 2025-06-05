"""Provides shellcheck dependencies on all supported platforms:
- Linux 64-bit and ARM64
- OSX 64-bit
"""

load("@rules_shellcheck//:deps.bzl", _deps = "shellcheck_dependencies")

def _impl(_):
    _deps()

shellcheck_dependencies = module_extension(
    implementation = _impl,
)
