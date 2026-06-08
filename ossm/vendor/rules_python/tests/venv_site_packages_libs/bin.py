import importlib
import sys
import unittest
from pathlib import Path


class VenvSitePackagesLibraryTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        if sys.prefix == sys.base_prefix:
            raise AssertionError("Not running under a venv")
        self.venv = sys.prefix

    def assert_imported_from_venv(self, module_name):
        module = importlib.import_module(module_name)
        self.assertEqual(module.__name__, module_name)
        self.assertIsNotNone(
            module.__file__,
            f"Expected module {module_name!r} to have"
            + f"__file__ set, but got None. {module=}",
        )
        self.assertTrue(
            module.__file__.startswith(self.venv),
            f"\n{module_name} was imported, but not from the venv.\n"
            + f"venv  : {self.venv}\n"
            + f"actual: {module.__file__}",
        )
        return module

    def test_imported_from_venv(self):
        m = self.assert_imported_from_venv("pkgutil_top")
        self.assertEqual(m.WHOAMI, "pkgutil_top")

        m = self.assert_imported_from_venv("pkgutil_top.sub")
        self.assertEqual(m.WHOAMI, "pkgutil_top.sub")

        self.assert_imported_from_venv("nspkg.subnspkg.alpha")
        self.assert_imported_from_venv("nspkg.subnspkg.beta")
        self.assert_imported_from_venv("nspkg.subnspkg.gamma")
        self.assert_imported_from_venv("nspkg.subnspkg.delta")
        self.assert_imported_from_venv("single_file")
        self.assert_imported_from_venv("simple")
        m = self.assert_imported_from_venv("nested_with_pth")
        self.assertEqual(m.WHOAMI, "nested_with_pth")

    def test_data_is_included(self):
        self.assert_imported_from_venv("simple")
        module = importlib.import_module("simple")
        module_path = Path(module.__file__)

        site_packages = module_path.parent.parent

        # Ensure that packages from simple v1 are not present
        files = [p.name for p in site_packages.glob("*")]
        self.assertIn("simple_v1_extras", files)

    def test_override_pkg(self):
        self.assert_imported_from_venv("simple")
        module = importlib.import_module("simple")
        self.assertEqual(
            "1.0.0",
            module.__version__,
        )

    def test_dirs_from_replaced_package_are_not_present(self):
        self.assert_imported_from_venv("simple")
        module = importlib.import_module("simple")
        module_path = Path(module.__file__)

        site_packages = module_path.parent.parent
        dist_info_dirs = [p.name for p in site_packages.glob("*.dist-info")]
        self.assertEqual(
            ["simple-1.0.0.dist-info"],
            dist_info_dirs,
        )

        # Ensure that packages from simple v1 are not present
        files = [p.name for p in site_packages.glob("*")]
        self.assertNotIn("simple.libs", files)

    def test_data_from_another_pkg_is_included_via_copy_file(self):
        self.assert_imported_from_venv("simple")
        module = importlib.import_module("simple")
        module_path = Path(module.__file__)

        site_packages = module_path.parent.parent
        # Ensure that packages from simple v1 are not present
        d = site_packages / "external_data"
        files = [p.name for p in d.glob("*")]
        self.assertIn("another_module_data.txt", files)


if __name__ == "__main__":
    unittest.main()
