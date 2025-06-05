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

"""Helper functions to preserve legacy `$` usage in templated_args
"""

def preserve_legacy_templated_args(input):
    """Converts legacy uses of `$` to `$$` so that the new call to ctx.expand_make_variables

    Converts any lone `$` that is not proceeded by a `(` to `$$. Also converts legacy `$(rlocation `
    to `$$(rlocation ` as that is a commonly used bash function so we don't want to break this
    legacy behavior.

    Args:
      input: String to be modified

    Returns:
      The modified string
    """
    result = ""
    length = len(input)
    for i in range(length):
        if input[i:].startswith("$(rlocation "):
            if i == 0 or input[i - 1] != "$":
                # insert an additional "$"
                result += "$"
        elif input[i] == "$" and (i + 1 == length or (i + 1 < length and input[i + 1] != "(" and input[i + 1] != "$")):
            if i == 0 or input[i - 1] != "$":
                # insert an additional "$"
                result += "$"
        result += input[i]
    return result
