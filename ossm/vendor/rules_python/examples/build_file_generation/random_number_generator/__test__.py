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

import random_number_generator.generate_random_number as generate_random_number


class TestRandomNumberGenerator(unittest.TestCase):
    def test_generate_random_number(self):
        number = generate_random_number.generate_random_number()
        self.assertGreaterEqual(number, 1)
        self.assertLessEqual(number, 10)


if __name__ == "__main__":
    unittest.main()
