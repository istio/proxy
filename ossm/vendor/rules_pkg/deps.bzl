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
"""Workspace dependencies for rules_pkg/pkg.
This is for backwards compatibility with existing usage.  Please use
     load("//pkg:deps.bzl", "rules_pkg_dependencies")
going forward.
"""

load("//pkg:deps.bzl", _rules_pkg_dependencies = "rules_pkg_dependencies")

rules_pkg_dependencies = _rules_pkg_dependencies

def rules_pkg_register_toolchains():
    pass
