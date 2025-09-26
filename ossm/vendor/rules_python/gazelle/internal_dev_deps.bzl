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
"""Module extension for internal dev_dependency=True setup."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

def internal_dev_deps():
    """This extension creates internal rules_python_gazelle dev dependencies."""
    http_file(
        name = "pytest",
        downloaded_file_path = "pytest-8.3.3-py3-none-any.whl",
        sha256 = "a6853c7375b2663155079443d2e45de913a911a11d669df02a50814944db57b2",
        urls = [
            "https://files.pythonhosted.org/packages/6b/77/7440a06a8ead44c7757a64362dd22df5760f9b12dc5f11b6188cd2fc27a0/pytest-8.3.3-py3-none-any.whl",
        ],
    )
    http_file(
        name = "django-types",
        downloaded_file_path = "django_types-0.19.1-py3-none-any.whl",
        sha256 = "b3f529de17f6374d41ca67232aa01330c531bbbaa3ac4097896f31ac33c96c30",
        urls = [
            "https://files.pythonhosted.org/packages/25/cb/d088c67245a9d5759a08dbafb47e040ee436e06ee433a3cdc7f3233b3313/django_types-0.19.1-py3-none-any.whl",
        ],
    )

def _internal_dev_deps_impl(mctx):
    _ = mctx  # @unused

    # This wheel is purely here to validate the wheel extraction code. It's not
    # intended for anything else.
    internal_dev_deps()

internal_dev_deps_extension = module_extension(
    implementation = _internal_dev_deps_impl,
    doc = "This extension creates internal rules_python_gazelle dev dependencies.",
)
