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

# This script takes a pair of Bazel-generated RPM %install scriptlet and %files
# list, and combined with JSON description of TreeArtifacts, emits copies
# augmented with the files detected in the materialized TreeArtifacts.

import os
import sys
import json

# NOTE: Keep those two in sync with the same variables in rpm_pfg.bzl
_INSTALL_FILE_STANZA_FMT = """
install -d "%{{buildroot}}/$(dirname '{1}')"
cp '{0}' '%{{buildroot}}/{1}'
""".strip()

_FILE_MODE_STANZA_FMT = """
{0} "{1}"
""".strip()

# Cheapo arg parsing.  Currently this script is single-purpose.

# JSON file containing the TreeArtifact manifest info.
#
# This is expected to be an array of objects with the following fields:
#
# - src: Source file/directory location.
# - dest: Install prefix
# - tags: Tags for the %files manifest
dir_data_path = sys.argv[1]

# Existing files
existing_install_script_path = sys.argv[2]
existing_files_path = sys.argv[3]

# Output files
new_install_script_path = sys.argv[4]
new_files_path = sys.argv[5]

# Computed outputs to be combined with the originals
dir_install_script_segments = []
dir_files_segments = []

with open(dir_data_path, 'r') as fh:
    dir_data = json.load(fh)

    for d in dir_data:
        # d is a dict, d["src"] is the TreeArtifact directory to walk.
        for root, dirs, files in os.walk(d["src"]):
            # "root" is the current directory we're walking through.  This
            # computes the path the source location (the TreeArtifact root) --
            # the desired install location relative to the user-provided install
            # destination.
            path_relative_to_install_dest = os.path.relpath(root, start=d["src"])

            for f in files:
                full_dest = os.path.join(d["dest"], path_relative_to_install_dest, f)
                dir_install_script_segments.append(_INSTALL_FILE_STANZA_FMT.format(
                    os.path.join(root, f),
                    full_dest
                ))
                dir_files_segments.append(_FILE_MODE_STANZA_FMT.format(d["tags"], full_dest))

with open(existing_install_script_path, 'r') as fh:
    existing_install_script = fh.read()

with open(existing_files_path, 'r') as fh:
    existing_files = fh.read()

# Write the outputs
with open(new_install_script_path, 'w') as fh:
    fh.write(existing_install_script)
    fh.write("\n")
    fh.write("\n".join(dir_install_script_segments))

with open(new_files_path, 'w') as fh:
    fh.write(existing_files)
    fh.write("\n")
    fh.write("\n".join(dir_files_segments))
