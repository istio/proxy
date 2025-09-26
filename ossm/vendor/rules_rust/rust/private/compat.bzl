# Copyright 2015 The Bazel Authors. All rights reserved.
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

"""Compatibility functions for older Bazel versions."""

# TODO: remove after dropping support for Bazel < 7 when `abs` is a global
def abs(value):
    """Returns the absolute value of a number.

    Args:
      value (int): A number.

    Returns:
      int: The absolute value of the number.
    """
    if value < 0:
        return -value
    return value
