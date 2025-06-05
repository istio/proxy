# Copyright 2017 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Skylib module containing a library rule for aggregating rules files."""

StarlarkLibraryInfo = provider(
    "Information on contained Starlark rules.",
    fields = {
        "srcs": "Top level rules files.",
        "transitive_srcs": "Transitive closure of rules files required for " +
                           "interpretation of the srcs",
    },
)

def _bzl_library_impl(ctx):
    deps_files = [x.files for x in ctx.attr.deps]
    all_files = depset(ctx.files.srcs, order = "postorder", transitive = deps_files)
    if not ctx.files.srcs and not deps_files:
        fail("bzl_library rule '%s' has no srcs or deps" % ctx.label)

    return [
        # All dependent files should be listed in both `files` and in `runfiles`;
        # this ensures that a `bzl_library` can be referenced as `data` from
        # a separate program, or from `tools` of a genrule().
        DefaultInfo(
            files = all_files,
            runfiles = ctx.runfiles(transitive_files = all_files),
        ),

        # We also define our own provider struct, for aggregation and testing.
        StarlarkLibraryInfo(
            srcs = ctx.files.srcs,
            transitive_srcs = all_files,
        ),
    ]

bzl_library = rule(
    implementation = _bzl_library_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".bzl", ".scl"],
            doc = "List of `.bzl` and `.scl` files that are processed to create this target.",
        ),
        "deps": attr.label_list(
            allow_files = [".bzl", ".scl"],
            doc = """List of other `bzl_library` or `filegroup` targets that are required by the
Starlark files listed in `srcs`.""",
        ),
    },
    doc = """Creates a logical collection of Starlark .bzl and .scl files.

Example:
  Suppose your project has the following structure:

  ```
  [workspace]/
      WORKSPACE
      BUILD
      checkstyle/
          BUILD
          checkstyle.bzl
      lua/
          BUILD
          lua.bzl
          luarocks.bzl
  ```

  In this case, you can have `bzl_library` targets in `checkstyle/BUILD` and
  `lua/BUILD`:

  `checkstyle/BUILD`:

  ```python
  load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

  bzl_library(
      name = "checkstyle-rules",
      srcs = ["checkstyle.bzl"],
  )
  ```

  `lua/BUILD`:

  ```python
  load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

  bzl_library(
      name = "lua-rules",
      srcs = [
          "lua.bzl",
          "luarocks.bzl",
      ],
  )
  ```
""",
)
