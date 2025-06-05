# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Starlark rules for integrating Sphinx and Readthedocs."""

load("//python:py_binary.bzl", "py_binary")
load("//python/private:util.bzl", "add_tag")  # buildifier: disable=bzl-visibility

_INSTALL_MAIN_SRC = Label("//sphinxdocs/private:readthedocs_install.py")

def readthedocs_install(name, docs, **kwargs):
    """Run a program to copy Sphinx doc files into readthedocs output directories.

    This is intended to be run using `bazel run` during the readthedocs
    build process when the build process is overridden. See
    https://docs.readthedocs.io/en/stable/build-customization.html#override-the-build-process
    for more information.

    Args:
        name: {type}`Name` name of the installer
        docs: {type}`list[label]` list of targets that generate directories to copy
            into the directories readthedocs expects final output in. This
            is typically a single {obj}`sphinx_stardocs` target.
        **kwargs: {type}`dict` additional kwargs to pass onto the installer
    """
    add_tag(kwargs, "@rules_python//sphinxdocs:readthedocs_install")
    py_binary(
        name = name,
        srcs = [_INSTALL_MAIN_SRC],
        main = _INSTALL_MAIN_SRC,
        data = docs,
        args = [
            "$(rlocationpaths {})".format(d)
            for d in docs
        ],
        deps = [Label("//python/runfiles")],
        **kwargs
    )
