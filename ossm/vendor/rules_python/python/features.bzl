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
_VERSION_PRIVATE = "1.6.3"

def _features_typedef():
    """Information about features rules_python has implemented.

    ::::{field} precompile
    :type: bool

    True if the precompile attributes are available.

    :::{versionadded} 0.33.0
    :::
    ::::

    ::::{field} py_info_venv_symlinks

    True if the `PyInfo.venv_symlinks` field is available.

    :::{versionadded} 1.5.0
    :::
    ::::

    ::::{field} uses_builtin_rules
    :type: bool

    True if the rules are using the Bazel-builtin implementation.

    :::{versionadded} 1.1.0
    :::
    ::::

    ::::{field} version
    :type: str

    The rules_python version. This is a semver format, e.g. `X.Y.Z` with
    optional trailing `-rcN`. For unreleased versions, it is an empty string.
    :::{versionadded} 0.38.0
    ::::
    """

features = struct(
    TYPEDEF = _features_typedef,
    # keep sorted
    precompile = True,
    py_info_venv_symlinks = True,
    uses_builtin_rules = not config.enable_pystar,
    version = _VERSION_PRIVATE if "$Format" not in _VERSION_PRIVATE else "",
)
