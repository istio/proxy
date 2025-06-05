import unittest

import requests


class TestDependencies(unittest.TestCase):
    def test_import(self):
        self.assertIsNotNone(requests.get)


if __name__ == "__main__":
    unittest.main()
