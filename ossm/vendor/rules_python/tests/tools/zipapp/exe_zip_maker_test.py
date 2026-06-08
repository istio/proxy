import hashlib
import pathlib
import shutil
import stat
import tempfile
import unittest

from tools.private.zipapp import exe_zip_maker


class ExeZipMakerTest(unittest.TestCase):
    def setUp(self):
        self.test_dir = pathlib.Path(tempfile.mkdtemp())
        self.preamble_path = self.test_dir / "preamble.txt"
        self.zip_path = self.test_dir / "data.zip"
        self.output_path = self.test_dir / "output.exe"

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def assertStartsWith(self, actual, expected):
        if not actual.startswith(expected):
            self.fail(f"{actual!r} does not start with {expected!r}")

    def test_create_exe_zip(self):
        # Create dummy zip file
        zip_content = b"PK\x03\x04dummyzipcontent"
        self.zip_path.write_bytes(zip_content)

        # Calculate expected hash
        expected_hash = hashlib.sha256(zip_content).hexdigest().encode("utf-8")

        # Create preamble with placeholder
        preamble_text = b"#!/bin/bash\nEXPECTED_HASH='%ZIP_HASH%'\n# ... logic ...\n"
        self.preamble_path.write_bytes(preamble_text)

        # Call create_exe_zip directly
        exe_zip_maker.create_exe_zip(
            str(self.preamble_path), str(self.zip_path), str(self.output_path)
        )

        # Verify output exists
        self.assertTrue(
            self.output_path.exists(),
            msg=f"Output path '{self.output_path}' should exist",
        )

        # Verify executable bit
        st = self.output_path.stat()
        self.assertTrue(
            st.st_mode & stat.S_IEXEC,
            msg=f"Output path '{self.output_path}' should be executable",
        )

        # Verify content
        content = self.output_path.read_bytes()

        # Split content back into preamble and zip
        # We know the preamble text length after substitution.
        expected_preamble = preamble_text.replace(b"%ZIP_HASH%", expected_hash)

        self.assertStartsWith(content, expected_preamble)
        self.assertTrue(
            content.endswith(zip_content),
            msg="Output content should end with the zip content",
        )
        self.assertEqual(len(content), len(expected_preamble) + len(zip_content))


if __name__ == "__main__":
    unittest.main()
