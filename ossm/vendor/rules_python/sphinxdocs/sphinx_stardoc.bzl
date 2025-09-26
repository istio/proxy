# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Rules to generate Sphinx-compatible documentation for bzl files."""

load("//sphinxdocs/private:sphinx_stardoc.bzl", _sphinx_stardoc = "sphinx_stardoc", _sphinx_stardocs = "sphinx_stardocs")

sphinx_stardocs = _sphinx_stardocs
sphinx_stardoc = _sphinx_stardoc
