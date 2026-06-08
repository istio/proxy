import os.path
import shutil
import subprocess
import sys
import tempfile
import unittest


class RepoToolchainTest(unittest.TestCase):
    maxDiff = None

    def test_python_from_repo_used(self):
        actual = os.path.realpath(sys._base_executable.lower())
        # Normalize case: Windows may have case differences
        self.assertIn("pbs_runtime", actual.lower())


if __name__ == "__main__":
    unittest.main()
