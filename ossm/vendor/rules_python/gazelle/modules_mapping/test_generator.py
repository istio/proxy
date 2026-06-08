import pathlib
import unittest

from generator import Generator


class GeneratorTest(unittest.TestCase):
    def test_generator(self):
        whl = pathlib.Path(__file__).parent / "pytest-8.3.3-py3-none-any.whl"
        gen = Generator(None, None, {}, False)
        gen.dig_wheel(whl)
        self.assertLessEqual(
            {
                "_pytest": "pytest",
                "_pytest.__init__": "pytest",
                "_pytest._argcomplete": "pytest",
                "_pytest.config.argparsing": "pytest",
            }.items(),
            gen.mapping.items(),
        )

    def test_stub_generator(self):
        whl = pathlib.Path(__file__).parent / "django_types-0.19.1-py3-none-any.whl"
        gen = Generator(None, None, {}, True)
        gen.dig_wheel(whl)
        self.assertLessEqual(
            {
                "django_types": "django_types",
            }.items(),
            gen.mapping.items(),
        )

    def test_stub_excluded(self):
        whl = pathlib.Path(__file__).parent / "django_types-0.19.1-py3-none-any.whl"
        gen = Generator(None, None, {}, False)
        gen.dig_wheel(whl)
        self.assertEqual(
            {}.items(),
            gen.mapping.items(),
        )


if __name__ == "__main__":
    unittest.main()
