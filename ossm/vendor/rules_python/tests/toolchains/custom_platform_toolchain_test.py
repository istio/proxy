import sys
import unittest


class VerifyCustomPlatformToolchainTest(unittest.TestCase):

    def test_custom_platform_interpreter_used(self):
        # We expect the repo name, and thus path, to have the
        # platform name in it.
        self.assertIn("linux-x86-install-only-stripped", sys._base_executable)
        print(sys._base_executable)


if __name__ == "__main__":
    unittest.main()
