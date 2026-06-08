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

import argparse
import json
import pathlib
import re
import sys
import zipfile


# Generator is the modules_mapping.json file generator.
class Generator:
    stderr = None
    output_file = None
    excluded_patterns = None

    def __init__(self, stderr, output_file, excluded_patterns, include_stub_packages):
        self.stderr = stderr
        self.output_file = output_file
        self.excluded_patterns = [re.compile(pattern) for pattern in excluded_patterns]
        self.include_stub_packages = include_stub_packages
        self.mapping = {}

    # dig_wheel analyses the wheel .whl file determining the modules it provides
    # by looking at the directory structure.
    def dig_wheel(self, whl):
        # Skip stubs and types wheels.
        wheel_name = get_wheel_name(whl)
        if self.include_stub_packages and (
            wheel_name.endswith(("_stubs", "_types"))
            or wheel_name.startswith(("types_", "stubs_"))
        ):
            self.mapping[wheel_name.lower()] = wheel_name.lower()
            return
        with zipfile.ZipFile(whl, "r") as zip_file:
            for path in zip_file.namelist():
                if is_metadata(path):
                    if data_has_purelib_or_platlib(path):
                        self.module_for_path(path, whl)
                    else:
                        continue
                else:
                    self.module_for_path(path, whl)

    def simplify(self):
        simplified = {}
        for module, wheel_name in sorted(self.mapping.items(), key=lambda x: x[0]):
            mod = module
            while True:
                if mod in simplified:
                    if simplified[mod] != wheel_name:
                        break
                    wheel_name = ""
                    break
                if mod.count(".") == 0:
                    break
                mod = mod.rsplit(".", 1)[0]
            if wheel_name:
                simplified[module] = wheel_name
        self.mapping = simplified

    def module_for_path(self, path, whl):
        ext = pathlib.Path(path).suffix
        if ext == ".py" or ext == ".so":
            if "purelib" in path or "platlib" in path:
                root = "/".join(path.split("/")[2:])
            else:
                root = path

            wheel_name = get_wheel_name(whl)

            if root.endswith("/__init__.py"):
                # Note the '/' here means that the __init__.py is not in the
                # root of the wheel, therefore we can index the directory
                # where this file is as an importable package.
                module = root[: -len("/__init__.py")].replace("/", ".")
                if not self.is_excluded(module):
                    self.mapping[module] = wheel_name

            # Always index the module file.
            if ext == ".so":
                # Also remove extra metadata that is embeded as part of
                # the file name as an extra extension.
                ext = "".join(pathlib.Path(root).suffixes)
            module = root[: -len(ext)].replace("/", ".")
            if not self.is_excluded(module):
                self.mapping[module] = wheel_name

    def is_excluded(self, module):
        for pattern in self.excluded_patterns:
            if pattern.search(module):
                return True
        return False

    def run(self, wheel: pathlib.Path) -> int:
        """
        Entrypoint for the generator.

        Args:
            wheel: The path to the wheel file (`.whl`)
        Returns:
            Exit code (for `sys.exit`)
        """
        try:
            self.dig_wheel(wheel)
        except AssertionError as error:
            print(error, file=self.stderr)
            return 1
        self.simplify()
        mapping_json = json.dumps(self.mapping)
        with open(self.output_file, "w") as f:
            f.write(mapping_json)
        return 0


def get_wheel_name(path):
    pp = pathlib.PurePath(path)
    if pp.suffix != ".whl":
        raise RuntimeError(
            "{} is not a valid wheel file name: the wheel doesn't follow ".format(
                pp.name
            )
            + "https://www.python.org/dev/peps/pep-0427/#file-name-convention"
        )
    return pp.name[: pp.name.find("-")]


# is_metadata checks if the path is in a metadata directory.
# Ref: https://www.python.org/dev/peps/pep-0427/#file-contents.
def is_metadata(path):
    top_level = path.split("/")[0].lower()
    return top_level.endswith(".dist-info") or top_level.endswith(".data")


# The .data is allowed to contain a full purelib or platlib directory
# These get unpacked into site-packages, so require indexing too.
# This is the same if "Root-Is-Purelib: true" is set and the files are at the root.
# Ref: https://peps.python.org/pep-0427/#what-s-the-deal-with-purelib-vs-platlib
def data_has_purelib_or_platlib(path):
    maybe_lib = path.split("/")[1].lower()
    return is_metadata(path) and (maybe_lib == "purelib" or maybe_lib == "platlib")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="generator",
        description="Generates the modules mapping used by the Gazelle manifest.",
    )
    parser.add_argument("--output_file", type=str)
    parser.add_argument("--include_stub_packages", action="store_true")
    parser.add_argument("--exclude_patterns", nargs="+", default=[])
    parser.add_argument("--wheel", type=pathlib.Path)
    args = parser.parse_args()
    generator = Generator(
        sys.stderr, args.output_file, args.exclude_patterns, args.include_stub_packages
    )
    sys.exit(generator.run(args.wheel))
