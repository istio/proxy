# Copyright 2024 The Bazel Authors. All rights reserved.
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

import os
import subprocess
import sys
import unittest


class InterpreterTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.interpreter = os.environ["PYTHON_BIN"]

        v = sys.version_info
        self.version = f"{v.major}.{v.minor}"

    def test_self_version(self):
        """Performs a sanity check on the Python version used for this test."""
        expected_version = os.environ["EXPECTED_SELF_VERSION"]
        self.assertEqual(expected_version, self.version)

    def test_interpreter_version(self):
        """Validates that we can successfully execute arbitrary code from the CLI."""
        expected_version = os.environ.get("EXPECTED_INTERPRETER_VERSION", self.version)

        try:
            result = subprocess.check_output(
                [self.interpreter],
                text=True,
                stderr=subprocess.STDOUT,
                input="\r".join(
                    [
                        "import sys",
                        "v = sys.version_info",
                        "print(f'version: {v.major}.{v.minor}')",
                    ]
                ),
            ).strip()
        except subprocess.CalledProcessError as error:
            print("OUTPUT:", error.stdout)
            raise

        self.assertEqual(result, f"version: {expected_version}")

    def test_json_tool(self):
        """Validates that we can successfully invoke a module from the CLI."""
        # Pass unformatted JSON to the json.tool module.
        try:
            result = subprocess.check_output(
                [
                    self.interpreter,
                    "-m",
                    "json.tool",
                ],
                text=True,
                stderr=subprocess.STDOUT,
                input='{"json":"obj"}',
            ).strip()
        except subprocess.CalledProcessError as error:
            print("OUTPUT:", error.stdout)
            raise

        # Validate that we get formatted JSON back.
        self.assertEqual(result, '{\n    "json": "obj"\n}')


if __name__ == "__main__":
    unittest.main()
