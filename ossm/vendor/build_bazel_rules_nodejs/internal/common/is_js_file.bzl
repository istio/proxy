# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Helper function and variable for determining JavaScript files."""

JS_FILE_EXTENSIONS = ["js", "jsx", "mjs", "cjs"]

def is_javascript_filename(filename, include_map_files = False):
    """Gets whether the specified filename corresponds to a JavaScript file.

    Args:
      filename: File name to check.
      include_map_files: Whether corresponding `.map` files should also return `True`.

    Returns:
      A boolean indicating whether the file corresponds to a JavaScript file.
    """
    for extension in JS_FILE_EXTENSIONS:
        if filename.endswith(".%s" % extension):
            return True
        if include_map_files and filename.endswith(".%s.map" % extension):
            return True
    return False

def is_javascript_file(file, include_map_files = False):
    """Gets whether the specified Bazel `File` corresponds to a JavaScript file."""
    return is_javascript_filename(file.basename, include_map_files)
