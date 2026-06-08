# Copyright 2022 The Bazel Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Files that will be made available as convenience targets under the bazel
# toolchain repository.

aliased_libs = [
    "omp",
]

aliased_tools = [
    "clang-apply-replacements",
    "clang-format",
    "clang-tidy",
    "clangd",
    "llvm-cov",
    "llvm-profdata",
    "llvm-symbolizer",
]
