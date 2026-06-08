# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Internal dependencies that are not Bazel modules."""

load("@bazel_features//:features.bzl", "bazel_features")
load("//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")

def _non_module_dependencies(mctx):
    rules_fuzzing_dependencies()

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return mctx.extension_metadata(reproducible = True)

    return None

non_module_dependencies = module_extension(_non_module_dependencies)
