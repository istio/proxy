# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Fronting macro for the py_cc_toolchain rule."""

load(":py_cc_toolchain_rule.bzl", _py_cc_toolchain = "py_cc_toolchain")
load(":util.bzl", "add_tag")

# A fronting macro is used because macros have user-observable behavior;
# using one from the onset avoids introducing those changes in the future.
def py_cc_toolchain(**kwargs):
    """Creates a py_cc_toolchain target.

    This is a macro around the {rule}`py_cc_toolchain` rule.

    Args:
        **kwargs: Keyword args to pass onto underlying {rule}`py_cc_toolchain` rule.
    """

    #  This tag is added to easily identify usages through other macros.
    add_tag(kwargs, "@rules_python//python:py_cc_toolchain")
    _py_cc_toolchain(**kwargs)
