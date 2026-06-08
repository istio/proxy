import pathlib
import unittest
import json
import tempfile

from merger import merge_modules_mappings


class MergerTest(unittest.TestCase):
    _tmpdir: tempfile.TemporaryDirectory

    def setUp(self) -> None:
        super().setUp()
        self._tmpdir = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        super().tearDown()
        self._tmpdir.cleanup()
        del self._tmpdir

    @property
    def tmppath(self) -> pathlib.Path:
        return pathlib.Path(self._tmpdir.name)

    def make_input(self, mapping: dict[str, str]) -> pathlib.Path:
        _fd, file = tempfile.mkstemp(suffix=".json", dir=self._tmpdir.name)
        path = pathlib.Path(file)
        path.write_text(json.dumps(mapping))
        return path

    def test_merger(self):
        output_path = self.tmppath / "output.json"
        merge_modules_mappings(
            [
                self.make_input(
                    {
                        "_pytest": "pytest",
                        "_pytest.__init__": "pytest",
                        "_pytest._argcomplete": "pytest",
                        "_pytest.config.argparsing": "pytest",
                    }
                ),
                self.make_input({"django_types": "django_types"}),
            ],
            output_path,
        )

        self.assertEqual(
            {
                "_pytest": "pytest",
                "_pytest.__init__": "pytest",
                "_pytest._argcomplete": "pytest",
                "_pytest.config.argparsing": "pytest",
                "django_types": "django_types",
            },
            json.loads(output_path.read_text()),
        )


if __name__ == "__main__":
    unittest.main()
