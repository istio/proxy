# Copyright 2025 The Bazel Authors. All rights reserved.
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
"""Paths utility functions."""

def is_path_absolute(path):
    """Checks whether a path is absolute or not.

    Note that is does not take the execution platform into account.

    This was implemented to replace skylib's paths.is_absolute, which
    only checks for the presence of a colon character in the second position
    of the paths

    Args:
      path: A path (as a string).

    Returns:
      `True` if `path` is an absolute path.
    """

    if path.startswith("/"):
        return True

    # Check for DOS-style absolute paths for Windows
    return len(path) >= 3 and \
           path[0].isalpha() and \
           path[1] == ":" and \
           path[2] in ("/", "\\")
