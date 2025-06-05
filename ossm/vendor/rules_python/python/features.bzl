# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Allows detecting of rules_python features that aren't easily detected."""

load("@rules_python_internal//:rules_python_config.bzl", "config")

# This is a magic string expanded by `git archive`, as set by `.gitattributes`
# See https://git-scm.com/docs/git-archive/2.29.0#Documentation/git-archive.txt-export-subst
_VERSION_PRIVATE = "1.2.0"

features = struct(
    version = _VERSION_PRIVATE if "$Format" not in _VERSION_PRIVATE else "",
    precompile = True,
    uses_builtin_rules = not config.enable_pystar,
)
