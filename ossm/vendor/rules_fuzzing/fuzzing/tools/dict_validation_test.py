# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Lint as: python3
"""
Unit tests for dict_validator.py
"""

import unittest
from fuzzing.tools.dict_validation import validate_line


class DictValidatorTest(unittest.TestCase):

    def test_plain_entries(self):
        self.assertTrue(validate_line('kw1="blah"'))
        self.assertTrue(validate_line('"0123456789"'))
        self.assertTrue(validate_line('"abcdefghijklmnopqrstuvwxyz"'))
        self.assertTrue(validate_line('"ABCDEFGHIJKLMNOPQRSTUVWXYZ"'))
        self.assertTrue(validate_line('"!"#$%&\'()*+,-./:;<=>?@[]^_`{|}~ "'))
        self.assertTrue(validate_line('"\t\r\f\v"'))

    def test_bad_chars(self):
        self.assertFalse(validate_line('"\x07"'))
        self.assertFalse(validate_line('"Ā"'))
        self.assertFalse(validate_line('"😀"'))

    def test_escaped_words(self):
        self.assertTrue(validate_line('kw2="\\"ac\\\\dc\\""'))
        self.assertTrue(validate_line('kw3="\\xF7\\xF8"'))
        self.assertTrue(validate_line('"foo\\x0Abar"'))

    def test_invalid_escaped_words(self):
        self.assertFalse(validate_line('"\\A"'))

    def test_unfinished_escape(self):
        self.assertFalse(validate_line('"\\"'))
        self.assertFalse(validate_line('"\\x"'))
        self.assertFalse(validate_line('"\\x1"'))

    def test_comment(self):
        self.assertTrue(validate_line('# valid dictionary entries'))

    def test_space_after_entry(self):
        self.assertTrue(validate_line('"entry" \t\r\f'))

    def test_nonspace_after_entry(self):
        self.assertFalse(validate_line('"entry"suffix'))

    def test_empty_entry(self):
        self.assertFalse(validate_line('""'))

    def test_empty_string(self):
        self.assertTrue(validate_line(''))

    def test_spaces(self):
        self.assertTrue(validate_line('   '))

    def test_plain_words(self):
        self.assertFalse(validate_line('Invalid dictionary entries'))

    def test_single_quote(self):
        self.assertFalse(validate_line('"'))


if __name__ == '__main__':
    unittest.main()
