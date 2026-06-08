# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Macro to extract tools from a directory."""

load("@bazel_skylib//rules/directory:glob.bzl", "directory_glob")
load(":tool.bzl", "cc_tool")

def cc_directory_tool(name, directory, executable, data = [], exclude = [], allow_empty = False, **kwargs):
    """A tool extracted from a directory.

    Args:
        name: (str) The name of the generated target
        directory: (Label) The directory to extract from
        executable: (str) The relative path from the directory to the
            executable.
        data: (List[str]) A list of globs to runfiles for the executable,
          relative to the directory.
        exclude: (List[str]) A list of globs to exclude from data.
        allow_empty: (bool) If false, any glob that fails to match anything will
          result in a failure.
        **kwargs: Kwargs to be passed through to cc_tool.
    """
    files_name = "_%s_files" % name
    directory_glob(
        name = files_name,
        directory = directory,
        srcs = [executable],
        data = data,
        exclude = exclude,
        allow_empty = allow_empty,
        visibility = ["//visibility:private"],
    )

    cc_tool(
        name = name,
        src = files_name,
        **kwargs
    )
