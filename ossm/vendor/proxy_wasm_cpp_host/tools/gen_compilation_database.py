#!/usr/bin/env python3

# Copyright 2016-2019 Envoy Project Authors
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import json
import os
import shlex
import subprocess
from pathlib import Path

# This is copied from https://github.com/envoyproxy/envoy and remove unnecessary code.

# This method is equivalent to https://github.com/grailbio/bazel-compilation-database/blob/master/generate.py
def generate_compilation_database(args):
    # We need to download all remote outputs for generated source code. This option lives here to override those
    # specified in bazelrc.
    bazel_startup_options = shlex.split(os.environ.get("BAZEL_STARTUP_OPTION_LIST", ""))
    bazel_options = shlex.split(os.environ.get("BAZEL_BUILD_OPTION_LIST", "")) + [
        "--remote_download_outputs=all",
    ]

    source_dir_targets = args.bazel_targets

    subprocess.check_call(["bazel", *bazel_startup_options, "build"] + bazel_options + [
        "--aspects=@bazel_compdb//:aspects.bzl%compilation_database_aspect",
        "--output_groups=compdb_files,header_files"
    ] + source_dir_targets)

    execroot = subprocess.check_output(
        ["bazel", *bazel_startup_options, "info", *bazel_options,
         "execution_root"]).decode().strip()

    db_entries = []
    for db in Path(execroot).glob('**/*.compile_commands.json'):
        db_entries.extend(json.loads(db.read_text()))

    def replace_execroot_marker(db_entry):
        if 'directory' in db_entry and db_entry['directory'] == '__EXEC_ROOT__':
            db_entry['directory'] = execroot
        if 'command' in db_entry:
            db_entry['command'] = (
                db_entry['command'].replace('-isysroot __BAZEL_XCODE_SDKROOT__', ''))
        return db_entry

    return list(map(replace_execroot_marker, db_entries))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate JSON compilation database')
    parser.add_argument('--include_external', action='store_true')
    parser.add_argument('--include_genfiles', action='store_true')
    parser.add_argument('--include_headers', action='store_true')
    parser.add_argument('--include_all', action='store_true')
    parser.add_argument(
        '--system-clang',
        action='store_true',
        help=
        'Use `clang++` instead of the bazel wrapper for commands. This may help if `clangd` cannot find/run the tools.'
    )
    parser.add_argument('bazel_targets', nargs='*', default=[])

    args = parser.parse_args()
    db = generate_compilation_database(args)

    with open("compile_commands.json", "w") as db_file:
        json.dump(db, db_file, indent=2)
