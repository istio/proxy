# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Implementation of py_library rule."""

load(":common.bzl", "collect_cc_info", "create_library_semantics_struct", "get_imports")
load(":precompile.bzl", "maybe_precompile")
load(":py_library.bzl", "create_py_library_rule", "py_library_impl")

def _py_library_impl_with_semantics(ctx):
    return py_library_impl(
        ctx,
        semantics = create_library_semantics_struct(
            get_imports = get_imports,
            maybe_precompile = maybe_precompile,
            get_cc_info_for_library = collect_cc_info,
        ),
    )

py_library = create_py_library_rule(
    implementation = _py_library_impl_with_semantics,
)
