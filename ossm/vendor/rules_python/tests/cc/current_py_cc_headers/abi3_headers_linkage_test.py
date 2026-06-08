import os.path
import pathlib
import sys
import unittest

import pefile

from python.runfiles import runfiles


class CheckLinkageTest(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith("win"), "requires windows")
    def test_linkage_windows(self):
        rf = runfiles.Create()
        dll_path = rf.Rlocation("rules_python/tests/cc/current_py_cc_headers/bin_abi3.dll")
        pe = pefile.PE(dll_path)
        if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
            self.fail("No import directory found.")

        imported_dlls = [
            entry.dll.decode("utf-8").lower() for entry in pe.DIRECTORY_ENTRY_IMPORT
        ]
        python_dlls = [dll for dll in imported_dlls if dll.startswith("python3")]
        self.assertEqual(python_dlls, ["python3.dll"])


if __name__ == "__main__":
    unittest.main()
