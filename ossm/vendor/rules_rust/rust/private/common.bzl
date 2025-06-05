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

"""A resilient API layer wrapping compilation and other logic for Rust rules.

This module is meant to be used by custom rules that need to compile Rust code
and cannot simply rely on writing a macro that wraps `rust_library`. This module
provides the lower-level interface to Rust providers, actions, and functions.
Do not load this file directly; instead, load the top-level `defs.bzl` file,
which exports the `rust_common` struct.

In the Bazel lingo, `rust_common` gives the access to the Rust Sandwich API.
"""

load(":providers.bzl", "CrateGroupInfo", "CrateInfo", "DepInfo", "DepVariantInfo", "StdLibInfo", "TestCrateInfo")

# This constant only represents the default value for attributes and macros
# defined in `rules_rust`. Like any attribute public attribute, it can be
# overwritten by the user on the rules they're defined on.
#
# Note: Code in `.github/workflows/crate_universe.yaml` looks for this line, if
# you remove it or change its format, you will also need to update that code.
DEFAULT_RUST_VERSION = "1.83.0"

DEFAULT_NIGHTLY_ISO_DATE = "2024-11-28"

def _create_crate_info(**kwargs):
    """A constructor for a `CrateInfo` provider

    This function should be used in place of directly creating a `CrateInfo`
    provider to improve API stability.

    Args:
        **kwargs: An inital set of keyword arguments.

    Returns:
        CrateInfo: A provider
    """
    if not "wrapped_crate_type" in kwargs:
        kwargs.update({"wrapped_crate_type": None})
    if not "metadata" in kwargs:
        kwargs.update({"metadata": None})
    if not "rustc_rmeta_output" in kwargs:
        kwargs.update({"rustc_rmeta_output": None})
    if not "rustc_output" in kwargs:
        kwargs.update({"rustc_output": None})
    if not "rustc_env_files" in kwargs:
        kwargs.update({"rustc_env_files": []})
    if not "data" in kwargs:
        kwargs.update({"data": depset([])})
    return CrateInfo(**kwargs)

rust_common = struct(
    create_crate_info = _create_crate_info,
    crate_info = CrateInfo,
    dep_info = DepInfo,
    dep_variant_info = DepVariantInfo,
    stdlib_info = StdLibInfo,
    test_crate_info = TestCrateInfo,
    crate_group_info = CrateGroupInfo,
    default_version = DEFAULT_RUST_VERSION,
)

COMMON_PROVIDERS = [
    CrateInfo,
    DepInfo,
    DefaultInfo,
]
