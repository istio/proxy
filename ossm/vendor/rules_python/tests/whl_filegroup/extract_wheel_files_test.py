import tempfile
import unittest
from pathlib import Path

from python.private.whl_filegroup import extract_wheel_files

_WHEEL = Path("examples/wheel/example_minimal_package-0.0.1-py3-none-any.whl")


class WheelRecordTest(unittest.TestCase):
    def test_get_wheel_record(self) -> None:
        record = extract_wheel_files.get_record(_WHEEL)
        expected = (
            "examples/wheel/lib/data,with,commas.txt",
            "examples/wheel/lib/data.txt",
            "examples/wheel/lib/module_with_data.py",
            "examples/wheel/lib/simple_module.py",
            "examples/wheel/main.py",
            "example_minimal_package-0.0.1.dist-info/WHEEL",
            "example_minimal_package-0.0.1.dist-info/METADATA",
            "example_minimal_package-0.0.1.dist-info/RECORD",
        )
        self.maxDiff = None
        self.assertEqual(list(record), list(expected))

    def test_get_files(self) -> None:
        pattern = "(examples/wheel/lib/.*\.txt$|.*main)"
        record = extract_wheel_files.get_record(_WHEEL)
        files = extract_wheel_files.get_files(record, pattern)
        expected = [
            "examples/wheel/lib/data,with,commas.txt",
            "examples/wheel/lib/data.txt",
            "examples/wheel/main.py",
        ]
        self.assertEqual(files, expected)

    def test_extract(self) -> None:
        files = {
            "examples/wheel/lib/data,with,commas.txt",
            "examples/wheel/lib/data.txt",
            "examples/wheel/main.py",
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            outdir = Path(tmpdir)
            extract_wheel_files.extract_files(_WHEEL, files, outdir)
            extracted_files = {
                f.relative_to(outdir).as_posix()
                for f in outdir.glob("**/*")
                if f.is_file()
            }
        self.assertEqual(extracted_files, files)


if __name__ == "__main__":
    unittest.main()
