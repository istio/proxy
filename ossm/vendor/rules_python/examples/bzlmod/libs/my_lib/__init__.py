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

import websockets


def websockets_is_for_python_version(sanitized_version_check):
    # We are checking that the name of the repository folders
    # match the expected generated names. If we update the folder
    # structure or naming we will need to modify this test.
    want = f"_{sanitized_version_check}_websockets"
    got_full = websockets.__file__
    if want not in got_full:
        print(f"Failed, expected '{want}' to be a substring of '{got_full}'.")
        return False

    return True
