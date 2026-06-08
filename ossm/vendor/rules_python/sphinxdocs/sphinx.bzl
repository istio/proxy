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

"""Rules to generate Sphinx documentation.

The general usage of the Sphinx rules requires two pieces:

1. Using `sphinx_docs` to define the docs to build and options for building.
2. Defining a `sphinx-build` binary to run Sphinx with the necessary
   dependencies to be used by (1); the `sphinx_build_binary` rule helps with
   this.

Defining your own `sphinx-build` binary is necessary because Sphinx uses
a plugin model to support extensibility.

The Sphinx integration is still experimental.
"""

load(
    "//sphinxdocs/private:sphinx.bzl",
    _sphinx_build_binary = "sphinx_build_binary",
    _sphinx_docs = "sphinx_docs",
    _sphinx_inventory = "sphinx_inventory",
    _sphinx_run = "sphinx_run",
)

sphinx_build_binary = _sphinx_build_binary
sphinx_docs = _sphinx_docs
sphinx_inventory = _sphinx_inventory
sphinx_run = _sphinx_run
