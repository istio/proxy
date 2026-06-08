# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""A small library for common arguments when updating files."""

import pathlib

from python.runfiles import runfiles


def path_from_runfiles(input: str) -> pathlib.Path:
    """A helper to create a path from runfiles.

    Args:
        input: the string input to construct a path.

    Returns:
        the pathlib.Path path to a file which is verified to exist.
    """
    path = pathlib.Path(runfiles.Create().Rlocation(input))
    if not path.exists():
        raise ValueError(f"Path '{path}' does not exist")

    return path
