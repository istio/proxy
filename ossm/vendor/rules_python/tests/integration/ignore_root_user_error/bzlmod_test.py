# Copyright 2024 The Bazel Authors. All rights reserved.
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

import json
import pathlib
import unittest

from python.runfiles import runfiles


class BzlmodTest(unittest.TestCase):
    def test_ignore_root_user_error_true_for_all_toolchains(self):
        rf = runfiles.Create()
        debug_path = pathlib.Path(
            rf.Rlocation("rules_python_bzlmod_debug/debug_info.json")
        )
        debug_info = json.loads(debug_path.read_bytes())
        actual = debug_info["toolchains_registered"]
        # Because the root module set ignore_root_user_error=True, that should
        # be the default for all other toolchains.
        for entry in actual:
            self.assertTrue(
                entry["ignore_root_user_error"],
                msg=f"Expected ignore_root_user_error=True, but got: {entry}",
            )


if __name__ == "__main__":
    unittest.main()
