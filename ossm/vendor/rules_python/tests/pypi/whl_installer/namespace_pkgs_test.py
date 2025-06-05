# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import pathlib
import shutil
import tempfile
import unittest
from typing import Optional, Set

from python.private.pypi.whl_installer import namespace_pkgs


class TempDir:
    def __init__(self) -> None:
        self.dir = tempfile.mkdtemp()

    def root(self) -> str:
        return self.dir

    def add_dir(self, rel_path: str) -> None:
        d = pathlib.Path(self.dir, rel_path)
        d.mkdir(parents=True)

    def add_file(self, rel_path: str, contents: Optional[str] = None) -> None:
        f = pathlib.Path(self.dir, rel_path)
        f.parent.mkdir(parents=True, exist_ok=True)
        if contents:
            with open(str(f), "w") as writeable_f:
                writeable_f.write(contents)
        else:
            f.touch()

    def remove(self) -> None:
        shutil.rmtree(self.dir)


class TestImplicitNamespacePackages(unittest.TestCase):
    def assertPathsEqual(self, actual: Set[pathlib.Path], expected: Set[str]) -> None:
        self.assertEqual(actual, {pathlib.Path(p) for p in expected})

    def test_in_current_directory(self) -> None:
        directory = TempDir()
        directory.add_file("foo/bar/biz.py")
        directory.add_file("foo/bee/boo.py")
        directory.add_file("foo/buu/__init__.py")
        directory.add_file("foo/buu/bii.py")
        cwd = os.getcwd()
        os.chdir(directory.root())
        expected = {
            "foo",
            "foo/bar",
            "foo/bee",
        }
        try:
            actual = namespace_pkgs.implicit_namespace_packages(".")
            self.assertPathsEqual(actual, expected)
        finally:
            os.chdir(cwd)
            directory.remove()

    def test_finds_correct_namespace_packages(self) -> None:
        directory = TempDir()
        directory.add_file("foo/bar/biz.py")
        directory.add_file("foo/bee/boo.py")
        directory.add_file("foo/buu/__init__.py")
        directory.add_file("foo/buu/bii.py")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/foo/bar",
            directory.root() + "/foo/bee",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_ignores_empty_directories(self) -> None:
        directory = TempDir()
        directory.add_file("foo/bar/biz.py")
        directory.add_dir("foo/cat")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/foo/bar",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_empty_case(self) -> None:
        directory = TempDir()
        directory.add_file("foo/__init__.py")
        directory.add_file("foo/bar/__init__.py")
        directory.add_file("foo/bar/biz.py")

        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertEqual(actual, set())

    def test_ignores_non_module_files_in_directories(self) -> None:
        directory = TempDir()
        directory.add_file("foo/__init__.pyi")
        directory.add_file("foo/py.typed")

        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertEqual(actual, set())

    def test_parent_child_relationship_of_namespace_pkgs(self):
        directory = TempDir()
        directory.add_file("foo/bar/biff/my_module.py")
        directory.add_file("foo/bar/biff/another_module.py")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/foo/bar",
            directory.root() + "/foo/bar/biff",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_parent_child_relationship_of_namespace_and_standard_pkgs(self):
        directory = TempDir()
        directory.add_file("foo/bar/biff/__init__.py")
        directory.add_file("foo/bar/biff/another_module.py")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/foo/bar",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_parent_child_relationship_of_namespace_and_nested_standard_pkgs(self):
        directory = TempDir()
        directory.add_file("foo/bar/__init__.py")
        directory.add_file("foo/bar/biff/another_module.py")
        directory.add_file("foo/bar/biff/__init__.py")
        directory.add_file("foo/bar/boof/big_module.py")
        directory.add_file("foo/bar/boof/__init__.py")
        directory.add_file("fim/in_a_ns_pkg.py")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/fim",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_recognized_all_nonstandard_module_types(self):
        directory = TempDir()
        directory.add_file("ayy/my_module.pyc")
        directory.add_file("bee/ccc/dee/eee.so")
        directory.add_file("eff/jee/aych.pyd")

        expected = {
            directory.root() + "/ayy",
            directory.root() + "/bee",
            directory.root() + "/bee/ccc",
            directory.root() + "/bee/ccc/dee",
            directory.root() + "/eff",
            directory.root() + "/eff/jee",
        }
        actual = namespace_pkgs.implicit_namespace_packages(directory.root())
        self.assertPathsEqual(actual, expected)

    def test_skips_ignored_directories(self):
        directory = TempDir()
        directory.add_file("foo/boo/my_module.py")
        directory.add_file("foo/bar/another_module.py")

        expected = {
            directory.root() + "/foo",
            directory.root() + "/foo/bar",
        }
        actual = namespace_pkgs.implicit_namespace_packages(
            directory.root(),
            ignored_dirnames=[directory.root() + "/foo/boo"],
        )
        self.assertPathsEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
