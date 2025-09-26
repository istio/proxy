import os
import unittest


class TestEmpty(unittest.TestCase):
    def test_lists(self):
        self.assertEqual("", os.environ["REQUIREMENTS"])
        self.assertEqual("", os.environ["REQUIREMENTS_WHL"])
        self.assertEqual("", os.environ["REQUIREMENTS_DATA"])


if __name__ == "__main__":
    unittest.main()
