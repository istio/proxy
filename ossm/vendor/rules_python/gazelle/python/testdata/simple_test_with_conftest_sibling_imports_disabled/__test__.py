import unittest

from __init__ import foo


class FooTest(unittest.TestCase):
    def test_foo(self):
        self.assertEqual("foo", foo())


if __name__ == "__main__":
    unittest.main()
