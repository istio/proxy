"""Provider for executable-specific information.

The `PyExecutableInfo` provider contains information about an executable that
isn't otherwise available from its public attributes or other providers.

It exposes information primarily useful for consumers to package the executable,
or derive a new executable from the base binary.
"""

load("//python/private:py_executable_info.bzl", _PyExecutableInfo = "PyExecutableInfo")

PyExecutableInfo = _PyExecutableInfo
