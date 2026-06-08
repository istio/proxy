#!/usr/bin/env python3

# Copyright 2021 The Bazel Authors. All rights reserved.
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

import sys
import os

"""Creates a directory containing some files with provided contents

Usage: ./this.py output_dir_name file1 contents1 ... fileN contentsN
"""

dirname = sys.argv[1]

files_contents_map = {}

# Simple way of grouping over pairs.  There are other ones, like
# https://stackoverflow.com/a/16789836, but they either requiring copying a
# bunch of code around or having something that's a smidge unreadable.
rest_args_iter = iter(sys.argv[2:])
for a in rest_args_iter:
    files_contents_map[a] = next(rest_args_iter)

os.makedirs(dirname, exist_ok=True)

for fname, contents in files_contents_map.items():
    path = os.path.join(dirname, fname)
    os.makedirs(
        os.path.dirname(path),
        exist_ok=True,
    )
    if contents.startswith('@@'):
        os.symlink(contents[2:], path)
    else:
        with open(path, 'w') as fh:
            fh.write(contents)
