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

"""
A simple test macro.
"""

load("//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

def py_console_script_binary_in_a_macro(name, pkg, **kwargs):
    """A simple macro to see that we can use our macro in a macro.

    Args:
        name, str: the name of the target
        pkg, str: the pkg target
        **kwargs, Any: extra kwargs passed through.
    """
    py_console_script_binary(
        name = name,
        pkg = Label(pkg),
        **kwargs
    )
