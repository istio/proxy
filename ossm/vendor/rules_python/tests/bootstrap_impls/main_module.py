import sys
import unittest


class MainModuleTest(unittest.TestCase):
    def test_run_as_module(self):
        self.assertIsNotNone(__spec__, "__spec__ was none")
        # If not run as a module, __spec__ is None
        self.assertNotEqual(__name__, __spec__.name)
        self.assertEqual(__spec__.name, "tests.bootstrap_impls.main_module")


if __name__ == "__main__":
    unittest.main()
else:
    # Guard against running it as a module in a non-main way.
    sys.exit(f"__name__ should be __main__, got {__name__}")
