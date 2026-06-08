# Copyright 2018 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import hashlib
import os
import platform
import stat
import subprocess
import unittest
import zipfile

from python.runfiles import runfiles


class WheelTest(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        super().setUp()
        self.runfiles = runfiles.Create()

    def _get_path(self, filename):
        runfiles_path = os.path.join("rules_python/examples/wheel", filename)
        path = self.runfiles.Rlocation(runfiles_path)
        # The runfiles API can return None if the path doesn't exist or
        # can't be resolved.
        if not path:
            raise AssertionError(f"Runfiles failed to resolve {runfiles_path}")
        elif not os.path.exists(path):
            # A non-None value doesn't mean the file actually exists, though
            raise AssertionError(
                f"Path {path} does not exist (from runfiles path {runfiles_path}"
            )
        else:
            return path

    def assertFileSha256Equal(self, filename, want):
        hash = hashlib.sha256()
        with open(filename, "rb") as f:
            while True:
                buf = f.read(2**20)
                if not buf:
                    break
                hash.update(buf)
        self.assertEqual(want, hash.hexdigest())

    def assertAllEntriesHasReproducibleMetadata(self, zf):
        for zinfo in zf.infolist():
            self.assertEqual(zinfo.date_time, (1980, 1, 1, 0, 0, 0), msg=zinfo.filename)
            self.assertEqual(zinfo.create_system, 3, msg=zinfo.filename)
            self.assertEqual(
                zinfo.external_attr,
                (stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO | stat.S_IFREG) << 16,
                msg=zinfo.filename,
            )
            self.assertEqual(
                zinfo.compress_type, zipfile.ZIP_DEFLATED, msg=zinfo.filename
            )

    def test_py_library_wheel(self):
        filename = self._get_path("example_minimal_library-0.0.1-py3-none-any.whl")
        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/module_with_type_annotations.py",
                    "examples/wheel/lib/module_with_type_annotations.pyi",
                    "examples/wheel/lib/simple_module.py",
                    "example_minimal_library-0.0.1.dist-info/WHEEL",
                    "example_minimal_library-0.0.1.dist-info/METADATA",
                    "example_minimal_library-0.0.1.dist-info/RECORD",
                ],
            )
        self.assertFileSha256Equal(
            filename, "ef5afd9f6c3ff569ef7e5b2799d3a2ec9675d029414f341e0abd7254d6b9a25d"
        )

    def test_py_package_wheel(self):
        filename = self._get_path(
            "example_minimal_package-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "examples/wheel/lib/data,with,commas.txt",
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/module_with_type_annotations.py",
                    "examples/wheel/lib/module_with_type_annotations.pyi",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    "example_minimal_package-0.0.1.dist-info/WHEEL",
                    "example_minimal_package-0.0.1.dist-info/METADATA",
                    "example_minimal_package-0.0.1.dist-info/RECORD",
                ],
            )
        self.assertFileSha256Equal(
            filename, "39bec133cf79431e8d057eae550cd91aa9dfbddfedb53d98ebd36e3ade2753d0"
        )

    def test_customized_wheel(self):
        filename = self._get_path(
            "example_customized-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "examples/wheel/lib/data,with,commas.txt",
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/module_with_type_annotations.py",
                    "examples/wheel/lib/module_with_type_annotations.pyi",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    "example_customized-0.0.1.dist-info/WHEEL",
                    "example_customized-0.0.1.dist-info/METADATA",
                    "example_customized-0.0.1.dist-info/entry_points.txt",
                    "example_customized-0.0.1.dist-info/NOTICE",
                    "example_customized-0.0.1.dist-info/README",
                    "example_customized-0.0.1.dist-info/RECORD",
                ],
            )
            record_contents = zf.read("example_customized-0.0.1.dist-info/RECORD")
            wheel_contents = zf.read("example_customized-0.0.1.dist-info/WHEEL")
            metadata_contents = zf.read("example_customized-0.0.1.dist-info/METADATA")
            entry_point_contents = zf.read(
                "example_customized-0.0.1.dist-info/entry_points.txt"
            )

            print(record_contents)
            self.assertEqual(
                record_contents,
                # The entries are guaranteed to be sorted.
                b"""\
"examples/wheel/lib/data,with,commas.txt",sha256=9vJKEdfLu8bZRArKLroPZJh1XKkK3qFMXiM79MBL2Sg,12
examples/wheel/lib/data.txt,sha256=9vJKEdfLu8bZRArKLroPZJh1XKkK3qFMXiM79MBL2Sg,12
examples/wheel/lib/module_with_data.py,sha256=8s0Khhcqz3yVsBKv2IB5u4l4TMKh7-c_V6p65WVHPms,637
examples/wheel/lib/module_with_type_annotations.py,sha256=2p_0YFT0TBUufbGCAR_u2vtxF1nM0lf3dX4VGeUtYq0,637
examples/wheel/lib/module_with_type_annotations.pyi,sha256=fja3ql_WRJ1qO8jyZjWWrTTMcg1J7EpOQivOHY_8vI4,630
examples/wheel/lib/simple_module.py,sha256=z2hwciab_XPNIBNH8B1Q5fYgnJvQTeYf0ZQJpY8yLLY,637
examples/wheel/main.py,sha256=mFiRfzQEDwCHr-WVNQhOH26M42bw1UMF6IoqvtuDTrw,1047
example_customized-0.0.1.dist-info/WHEEL,sha256=sobxWSyDDkdg_rinUth-jxhXHqoNqlmNMJY3aTZn2Us,91
example_customized-0.0.1.dist-info/METADATA,sha256=QYQcDJFQSIqan8eiXqL67bqsUfgEAwf2hoK_Lgi1S-0,559
example_customized-0.0.1.dist-info/entry_points.txt,sha256=pqzpbQ8MMorrJ3Jp0ntmpZcuvfByyqzMXXi2UujuXD0,137
example_customized-0.0.1.dist-info/NOTICE,sha256=Xpdw-FXET1IRgZ_wTkx1YQfo1-alET0FVf6V1LXO4js,76
example_customized-0.0.1.dist-info/README,sha256=WmOFwZ3Jga1bHG3JiGRsUheb4UbLffUxyTdHczS27-o,40
example_customized-0.0.1.dist-info/RECORD,,
""",
            )
            self.assertEqual(
                wheel_contents,
                b"""\
Wheel-Version: 1.0
Generator: bazel-wheelmaker 1.0
Root-Is-Purelib: true
Tag: py3-none-any
""",
            )
            self.assertEqual(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: example_customized
Author: Example Author with non-ascii characters: \xc5\xbc\xc3\xb3\xc5\x82w
Author-email: example@example.com
Home-page: www.example.com
License: Apache 2.0
Description-Content-Type: text/markdown
Summary: A one-line summary of this test package
Project-URL: Bug Tracker, www.example.com/issues
Project-URL: Documentation, www.example.com/docs
Classifier: License :: OSI Approved :: Apache Software License
Classifier: Intended Audience :: Developers
Requires-Dist: pytest
Version: 0.0.1

This is a sample description of a wheel.
""",
            )
            self.assertEqual(
                entry_point_contents,
                b"""\
[console_scripts]
another = foo.bar:baz
customized_wheel = examples.wheel.main:main

[group2]
first = first.main:f
second = second.main:s""",
            )
        self.assertFileSha256Equal(
            filename, "685f68fc6665f53c9b769fd1ba12cce9937ab7f40ef4e60c82ef2de8653935de"
        )

    def test_filename_escaping(self):
        filename = self._get_path(
            "file_name_escaping-0.0.1rc1+ubuntu.r7-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertEqual(
                zf.namelist(),
                [
                    "examples/wheel/lib/data,with,commas.txt",
                    "examples/wheel/lib/data.txt",
                    "examples/wheel/lib/module_with_data.py",
                    "examples/wheel/lib/module_with_type_annotations.py",
                    "examples/wheel/lib/module_with_type_annotations.pyi",
                    "examples/wheel/lib/simple_module.py",
                    "examples/wheel/main.py",
                    # PEP calls for replacing only in the archive filename.
                    # Alas setuptools also escapes in the dist-info directory
                    # name, so let's be compatible.
                    "file_name_escaping-0.0.1rc1+ubuntu.r7.dist-info/WHEEL",
                    "file_name_escaping-0.0.1rc1+ubuntu.r7.dist-info/METADATA",
                    "file_name_escaping-0.0.1rc1+ubuntu.r7.dist-info/RECORD",
                ],
            )
            metadata_contents = zf.read(
                "file_name_escaping-0.0.1rc1+ubuntu.r7.dist-info/METADATA"
            )
            self.assertEqual(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: File--Name-Escaping
Version: 0.0.1rc1+ubuntu.r7

UNKNOWN
""",
            )

    def test_custom_package_root_wheel(self):
        filename = self._get_path(
            "examples_custom_package_root-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "wheel/lib/data,with,commas.txt",
                    "wheel/lib/data.txt",
                    "wheel/lib/module_with_data.py",
                    "wheel/lib/module_with_type_annotations.py",
                    "wheel/lib/module_with_type_annotations.pyi",
                    "wheel/lib/simple_module.py",
                    "wheel/main.py",
                    "examples_custom_package_root-0.0.1.dist-info/WHEEL",
                    "examples_custom_package_root-0.0.1.dist-info/METADATA",
                    "examples_custom_package_root-0.0.1.dist-info/entry_points.txt",
                    "examples_custom_package_root-0.0.1.dist-info/RECORD",
                ],
            )

            record_contents = zf.read(
                "examples_custom_package_root-0.0.1.dist-info/RECORD"
            ).decode("utf-8")

            # Ensure RECORD files do not have leading forward slashes
            for line in record_contents.splitlines():
                self.assertFalse(line.startswith("/"))
        self.assertFileSha256Equal(
            filename, "2fbfc3baaf6fccca0f97d02316b8344507fe6c8136991a66ee5f162235adb19f"
        )

    def test_custom_package_root_multi_prefix_wheel(self):
        filename = self._get_path(
            "example_custom_package_root_multi_prefix-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "data,with,commas.txt",
                    "data.txt",
                    "module_with_data.py",
                    "module_with_type_annotations.py",
                    "module_with_type_annotations.pyi",
                    "simple_module.py",
                    "main.py",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/WHEEL",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/METADATA",
                    "example_custom_package_root_multi_prefix-0.0.1.dist-info/RECORD",
                ],
            )

            record_contents = zf.read(
                "example_custom_package_root_multi_prefix-0.0.1.dist-info/RECORD"
            ).decode("utf-8")

            # Ensure RECORD files do not have leading forward slashes
            for line in record_contents.splitlines():
                self.assertFalse(line.startswith("/"))
        self.assertFileSha256Equal(
            filename, "3e67971ca1e8a9ba36a143df7532e641f5661c56235e41d818309316c955ba58"
        )

    def test_custom_package_root_multi_prefix_reverse_order_wheel(self):
        filename = self._get_path(
            "example_custom_package_root_multi_prefix_reverse_order-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "lib/data,with,commas.txt",
                    "lib/data.txt",
                    "lib/module_with_data.py",
                    "lib/module_with_type_annotations.py",
                    "lib/module_with_type_annotations.pyi",
                    "lib/simple_module.py",
                    "main.py",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/WHEEL",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/METADATA",
                    "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/RECORD",
                ],
            )

            record_contents = zf.read(
                "example_custom_package_root_multi_prefix_reverse_order-0.0.1.dist-info/RECORD"
            ).decode("utf-8")

            # Ensure RECORD files do not have leading forward slashes
            for line in record_contents.splitlines():
                self.assertFalse(line.startswith("/"))
        self.assertFileSha256Equal(
            filename, "372ef9e11fb79f1952172993718a326b5adda192d94884b54377c34b44394982"
        )

    def test_python_requires_wheel(self):
        filename = self._get_path(
            "example_python_requires_in_a_package-0.0.1-py3-none-any.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_contents = zf.read(
                "example_python_requires_in_a_package-0.0.1.dist-info/METADATA"
            )
            # The entries are guaranteed to be sorted.
            self.assertEqual(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: example_python_requires_in_a_package
Requires-Python: >=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*, !=3.4.*
Version: 0.0.1

UNKNOWN
""",
            )
        self.assertFileSha256Equal(
            filename, "10a325ba8f77428b5cfcff6345d508f5eb77c140889eb62490d7382f60d4ebfe"
        )

    def test_python_abi3_binary_wheel(self):
        arch = "amd64"
        if platform.system() != "Windows":
            arch = subprocess.check_output(["uname", "-m"]).strip().decode()
        # These strings match the strings from py_wheel() in BUILD
        os_strings = {
            "Linux": "manylinux2014",
            "Darwin": "macosx_11_0",
            "Windows": "win",
        }
        os_string = os_strings[platform.system()]
        filename = self._get_path(
            f"example_python_abi3_binary_wheel-0.0.1-cp38-abi3-{os_string}_{arch}.whl",
        )
        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_contents = zf.read(
                "example_python_abi3_binary_wheel-0.0.1.dist-info/METADATA"
            )
            # The entries are guaranteed to be sorted.
            self.assertEqual(
                metadata_contents,
                b"""\
Metadata-Version: 2.1
Name: example_python_abi3_binary_wheel
Requires-Python: >=3.8
Version: 0.0.1

UNKNOWN
""",
            )
            wheel_contents = zf.read(
                "example_python_abi3_binary_wheel-0.0.1.dist-info/WHEEL"
            )
            self.assertEqual(
                wheel_contents.decode(),
                f"""\
Wheel-Version: 1.0
Generator: bazel-wheelmaker 1.0
Root-Is-Purelib: false
Tag: cp38-abi3-{os_string}_{arch}
""",
            )

    def test_rule_creates_directory_and_is_included_in_wheel(self):
        filename = self._get_path(
            "use_rule_with_dir_in_outs-0.0.1-py3-none-any.whl",
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            self.assertEqual(
                zf.namelist(),
                [
                    "examples/wheel/main.py",
                    "examples/wheel/someDir/foo.py",
                    "use_rule_with_dir_in_outs-0.0.1.dist-info/WHEEL",
                    "use_rule_with_dir_in_outs-0.0.1.dist-info/METADATA",
                    "use_rule_with_dir_in_outs-0.0.1.dist-info/RECORD",
                ],
            )
        self.assertFileSha256Equal(
            filename, "85e44c43cc19ccae9fe2e1d629230203aa11791bed1f7f68a069fb58d1c93cd2"
        )

    def test_rule_expands_workspace_status_keys_in_wheel_metadata(self):
        filename = self._get_path(
            "example_minimal_library{BUILD_USER}-0.1.{BUILD_TIMESTAMP}-py3-none-any.whl"
        )

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                self.assertNotIn("{BUILD_TIMESTAMP}", f)
                self.assertNotIn("{BUILD_USER}", f)
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            version = None
            name = None
            with zf.open(metadata_file) as fp:
                for line in fp:
                    if line.startswith(b"Version:"):
                        version = line.decode().split()[-1]
                    if line.startswith(b"Name:"):
                        name = line.decode().split()[-1]
            self.assertIsNotNone(version)
            self.assertIsNotNone(name)
            self.assertNotIn("{BUILD_TIMESTAMP}", version)
            self.assertNotIn("{BUILD_USER}", name)

    def test_requires_file_and_extra_requires_files(self):
        filename = self._get_path("requires_files-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            requires = []
            with zf.open(metadata_file) as fp:
                for line in fp:
                    if line.startswith(b"Requires-Dist:"):
                        requires.append(line.decode("utf-8").strip())

            self.assertEqual(
                [
                    "Requires-Dist: tomli>=2.0.0",
                    "Requires-Dist: starlark",
                    "Requires-Dist: pyyaml!=6.0.1,>=6.0.0; extra == 'example'",
                    'Requires-Dist: toml; ((python_version == "3.11" or python_version == "3.12") and python_version != "3.8") and extra == \'example\'',
                    'Requires-Dist: wheel; (python_version == "3.11" or python_version == "3.12") and extra == \'example\'',
                ],
                requires,
            )

    def test_empty_requires_file(self):
        filename = self._get_path("empty_requires_files-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            metadata = zf.read(metadata_file).decode("utf-8")
            metadata_lines = metadata.splitlines()

            requires = []
            for i, line in enumerate(metadata_lines):
                if line.startswith("Name:"):
                    self.assertTrue(metadata_lines[i + 1].startswith("Version:"))
                if line.startswith("Requires-Dist:"):
                    requires.append(line.strip())

            self.assertEqual([], requires)

    def test_minimal_data_files(self):
        filename = self._get_path("minimal_data_files-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            self.assertEqual(
                zf.namelist(),
                [
                    "minimal_data_files-0.0.1.dist-info/WHEEL",
                    "minimal_data_files-0.0.1.dist-info/METADATA",
                    "minimal_data_files-0.0.1.data/data/target/path/README.md",
                    "minimal_data_files-0.0.1.data/scripts/NOTICE",
                    "minimal_data_files-0.0.1.dist-info/RECORD",
                ],
            )

    def test_extra_requires(self):
        filename = self._get_path("extra_requires-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            requires = []
            with zf.open(metadata_file) as fp:
                for line in fp:
                    if line.startswith(b"Requires-Dist:"):
                        requires.append(line.decode("utf-8").strip())

            print(requires)
            self.assertEqual(
                [
                    "Requires-Dist: tomli>=2.0.0",
                    "Requires-Dist: starlark",
                    'Requires-Dist: pytest; python_version != "3.8"',
                    "Requires-Dist: pyyaml!=6.0.1,>=6.0.0; extra == 'example'",
                    'Requires-Dist: toml; ((python_version == "3.11" or python_version == "3.12") and python_version != "3.8") and extra == \'example\'',
                    'Requires-Dist: wheel; (python_version == "3.11" or python_version == "3.12") and extra == \'example\'',
                ],
                requires,
            )

    def test_requires_dist_depends_on_extras(self):
        filename = self._get_path("requires_dist_depends_on_extras-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            requires = []
            with zf.open(metadata_file) as fp:
                for line in fp:
                    if line.startswith(b"Requires-Dist:"):
                        requires.append(line.decode("utf-8").strip())

            print(requires)
            self.assertEqual(
                [
                    "Requires-Dist: extra_requires[example]==0.0.1",
                ],
                requires,
            )

    def test_requires_dist_depends_on_extras_file(self):
        filename = self._get_path("requires_dist_depends_on_extras_using_file-0.0.1-py3-none-any.whl")

        with zipfile.ZipFile(filename) as zf:
            self.assertAllEntriesHasReproducibleMetadata(zf)
            metadata_file = None
            for f in zf.namelist():
                if os.path.basename(f) == "METADATA":
                    metadata_file = f
            self.assertIsNotNone(metadata_file)

            requires = []
            with zf.open(metadata_file) as fp:
                for line in fp:
                    if line.startswith(b"Requires-Dist:"):
                        requires.append(line.decode("utf-8").strip())

            print(requires)
            self.assertEqual(
                [
                    "Requires-Dist: extra_requires[example]==0.0.1",
                ],
                requires,
            )


if __name__ == "__main__":
    unittest.main()
