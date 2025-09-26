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

"""Wrapper for repository which call"""

_binary_not_found_msg = "Unable to find the binary '{binary_name}'.  Please update your PATH to include '{binary_name}'."

def which_with_fail(binary_name, rctx):
    """Tests to see if a binary exists, and otherwise fails with a message.

    Args:
        binary_name: name of the binary to find.
        rctx: repository context.

    Returns:
        rctx.Path for the binary.
    """
    binary = rctx.which(binary_name)
    if binary == None:
        fail(_binary_not_found_msg.format(binary_name = binary_name))
    return binary
