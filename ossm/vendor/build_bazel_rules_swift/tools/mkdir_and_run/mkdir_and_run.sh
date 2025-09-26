#!/bin/bash
#
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

# SYNOPSIS
#   Ensures (deeply) that a directory exists and then executes another command.
#
# USAGE
#   mkdir_and_run.sh <directory_path> <executable> <arguments...>
#
# ARGUMENTS
#   directory_path: The path to the directory that should be created if it does
#       not already exist.
#   executable: The path to the executable to invoke.
#   arguments...: Arguments passed directly to the invoked command.

set -eu

if [[ $# -lt 2 ]] ; then
  echo "ERROR: Need at least two arguments." 1>&2
  exit 1
fi

mkdir -p "$1"
shift
exec "$@"

# Should never get here.
exit 2
