# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest

from __init__ import app


class TestServer(unittest.TestCase):
    def setUp(self):
        self.app = app.test_client()

    def test_get_random_number(self):
        response = self.app.get("/random-number")
        self.assertEqual(response.status_code, 200)
        self.assertIn("number", response.json)


if __name__ == "__main__":
    unittest.main()
