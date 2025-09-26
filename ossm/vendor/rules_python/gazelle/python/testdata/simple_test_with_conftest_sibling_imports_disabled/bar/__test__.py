import unittest

from __init__ import bar


class BarTest(unittest.TestCase):
    def test_bar(self):
        self.assertEqual("bar", bar())


if __name__ == "__main__":
    unittest.main()
