# Copyright 2025 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Macro to build a python C/C++ extension.

There are variants of py_extension in many other projects, such as:
* https://github.com/protocolbuffers/protobuf/tree/main/python/py_extension.bzl
* https://github.com/google/riegeli/blob/master/python/riegeli/py_extension.bzl
* https://github.com/pybind/pybind11_bazel/blob/master/build_defs.bzl

The issue for a generic verion is:
* https://github.com/bazel-contrib/rules_python/issues/824
"""

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_python//python:defs.bzl", "py_library")

def py_extension(
        *,
        name,
        deps = None,
        linkopts = None,
        imports = None,
        visibility = None,
        **kwargs):
    """Creates a Python module implemented in C++.

    A Python extension has 2 essential parts:
      1.  An internal shared object / pyd package for the extension, `name.pyd`/`name.so`
      2.  The py_library target for the extension.`

    Python modules can depend on a py_extension.

    Args:
      name: `str`. Name for this target.  This is typically the module name.
      deps: `list`. Required. C++ libraries to link into the module.
      linkopts: `list`. Linking options for the shared library.
      imports: `list`. Additional imports for the py_library rule.
      visibility: `str`. Visibility for target.
      **kwargs:  Additional options for the cc_library rule.
    """
    if not name:
        fail("py_extension requires a name")
    if not deps:
        fail("py_extension requires a non-empty deps attribute")
    if "linkshared" in kwargs:
        fail("py_extension attribute linkshared not allowed")

    if not linkopts:
        linkopts = []

    testonly = kwargs.get("testonly")
    tags = kwargs.pop("tags", [])

    cc_binary_so_name = name + ".so"
    cc_binary_dll_name = name + ".dll"
    cc_binary_pyd_name = name + ".pyd"
    linker_script_name = name + ".lds"
    linker_script_name_rule = name + "_lds"
    shared_objects_name = name + "__shared_objects"

    # On Unix, restrict symbol visibility.
    exported_symbol = "PyInit_" + name

    # Generate linker script used on non-macOS unix platforms.
    native.genrule(
        name = linker_script_name_rule,
        outs = [linker_script_name],
        cmd = "\n".join([
            "cat <<'EOF' >$@",
            "{",
            "  global: " + exported_symbol + ";",
            "  local: *;",
            "};",
            "EOF",
        ]),
    )

    for cc_binary_name in [cc_binary_dll_name, cc_binary_so_name]:
        cur_linkopts = linkopts
        cur_deps = deps
        if cc_binary_name == cc_binary_so_name:
            cur_linkopts = linkopts + select({
                "@platforms//os:macos": [
                    # Avoid undefined symbol errors for CPython symbols that
                    # will be resolved at runtime.
                    "-undefined",
                    "dynamic_lookup",
                    # On macOS, the linker does not support version scripts.  Use
                    # the `-exported_symbol` option instead to restrict symbol
                    # visibility.
                    "-Wl,-exported_symbol",
                    # On macOS, the symbol starts with an underscore.
                    "-Wl,_" + exported_symbol,
                ],
                # On non-macOS unix, use a version script to restrict symbol
                # visibility.
                "//conditions:default": [
                    "-Wl,--version-script",
                    "-Wl,$(location :" + linker_script_name + ")",
                ],
            })
            cur_deps = cur_deps + select({
                "@platforms//os:macos": [],
                "//conditions:default": [linker_script_name],
            })

        cc_binary(
            name = cc_binary_name,
            linkshared = True,
            visibility = ["//visibility:private"],
            deps = cur_deps,
            tags = tags + ["manual"],
            linkopts = cur_linkopts,
            **kwargs
        )

    copy_file(
        name = cc_binary_pyd_name + "__pyd_copy",
        src = ":" + cc_binary_dll_name,
        out = cc_binary_pyd_name,
        visibility = visibility,
        tags = ["manual"],
        testonly = testonly,
    )

    native.filegroup(
        name = shared_objects_name,
        data = select({
            "@platforms//os:windows": [":" + cc_binary_pyd_name],
            "//conditions:default": [":" + cc_binary_so_name],
        }),
        testonly = testonly,
    )
    py_library(
        name = name,
        data = [":" + shared_objects_name],
        imports = imports,
        tags = tags,
        testonly = testonly,
        visibility = visibility,
    )
