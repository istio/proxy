# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Helper function to accept either a label or a directory_file_path as dict
"""

load("@rules_nodejs//nodejs:directory_file_path.bzl", "directory_file_path")

def maybe_directory_file_path(name, value):
    """Pass-through a value or convert a dict with a single key/value pair to a directory_file_path and return its label
    """
    if value and type(value) == "dict":
        # convert {"//directory/artifact:target": "file/path"} to
        # a directory_file_path value
        keys = value.keys()
        if len(keys) != 1:
            fail("Expected a dict with a single entry that corresponds to the directory_file_path directory key & path value")
        directory_file_path(
            name = "%s_directory_file_path" % name,
            directory = keys[0],
            path = value[keys[0]],
        )
        value = ":%s_directory_file_path" % name
    return value
