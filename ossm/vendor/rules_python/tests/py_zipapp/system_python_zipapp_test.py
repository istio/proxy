import os
import subprocess
import unittest
import zipfile


class SystemPythonZipAppTest(unittest.TestCase):
    def test_zipapp_runnable(self):
        zipapp_path = os.environ["TEST_ZIPAPP"]

        self.assertTrue(os.path.exists(zipapp_path))
        self.assertTrue(os.path.isfile(zipapp_path))

        output = subprocess.check_output([zipapp_path]).decode("utf-8").strip()
        self.assertIn("Hello from zipapp", output)
        self.assertIn("dep:", output)


if __name__ == "__main__":
    unittest.main()
