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

"""Public entry point to all Rust rules and supported APIs."""

load(
    "//rust:toolchain.bzl",
    _rust_stdlib_filegroup = "rust_stdlib_filegroup",
)
load(
    "//rust/private:clippy.bzl",
    _capture_clippy_output = "capture_clippy_output",
    _clippy_flag = "clippy_flag",
    _clippy_flags = "clippy_flags",
    _rust_clippy = "rust_clippy",
    _rust_clippy_aspect = "rust_clippy_aspect",
)
load("//rust/private:common.bzl", _rust_common = "rust_common")
load(
    "//rust/private:rust.bzl",
    _rust_binary = "rust_binary",
    _rust_library = "rust_library",
    _rust_library_group = "rust_library_group",
    _rust_proc_macro = "rust_proc_macro",
    _rust_shared_library = "rust_shared_library",
    _rust_static_library = "rust_static_library",
    _rust_test = "rust_test",
    _rust_test_suite = "rust_test_suite",
)
load(
    "//rust/private:rust_analyzer.bzl",
    _rust_analyzer_aspect = "rust_analyzer_aspect",
)
load(
    "//rust/private:rustc.bzl",
    _error_format = "error_format",
    _extra_exec_rustc_flag = "extra_exec_rustc_flag",
    _extra_exec_rustc_flags = "extra_exec_rustc_flags",
    _extra_rustc_flag = "extra_rustc_flag",
    _extra_rustc_flags = "extra_rustc_flags",
    _no_std = "no_std",
    _per_crate_rustc_flag = "per_crate_rustc_flag",
    _rustc_output_diagnostics = "rustc_output_diagnostics",
)
load(
    "//rust/private:rustdoc.bzl",
    _rust_doc = "rust_doc",
)
load(
    "//rust/private:rustdoc_test.bzl",
    _rust_doc_test = "rust_doc_test",
)
load(
    "//rust/private:rustfmt.bzl",
    _rustfmt_aspect = "rustfmt_aspect",
    _rustfmt_test = "rustfmt_test",
)
load(
    "//rust/private:unpretty.bzl",
    _rust_unpretty = "rust_unpretty",
    _rust_unpretty_aspect = "rust_unpretty_aspect",
)

rust_library = _rust_library
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_static_library = _rust_static_library
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_shared_library = _rust_shared_library
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_proc_macro = _rust_proc_macro
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_binary = _rust_binary
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_library_group = _rust_library_group
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_test = _rust_test
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_test_suite = _rust_test_suite
# See @rules_rust//rust/private:rust.bzl for a complete description.

rust_doc = _rust_doc
# See @rules_rust//rust/private:rustdoc.bzl for a complete description.

rust_doc_test = _rust_doc_test
# See @rules_rust//rust/private:rustdoc_test.bzl for a complete description.

clippy_flag = _clippy_flag
clippy_flags = _clippy_flags
# See @rules_rust//rust/private:clippy.bzl for a complete description.

rust_clippy_aspect = _rust_clippy_aspect
# See @rules_rust//rust/private:clippy.bzl for a complete description.

rust_clippy = _rust_clippy
# See @rules_rust//rust/private:clippy.bzl for a complete description.

capture_clippy_output = _capture_clippy_output
# See @rules_rust//rust/private:clippy.bzl for a complete description.

rustc_output_diagnostics = _rustc_output_diagnostics
# See @rules_rust//rust/private:rustc.bzl for a complete description.

rust_unpretty_aspect = _rust_unpretty_aspect
# See @rules_rust//rust/private:unpretty.bzl for a complete description.

rust_unpretty = _rust_unpretty
# See @rules_rust//rust/private:unpretty.bzl for a complete description.

error_format = _error_format
# See @rules_rust//rust/private:rustc.bzl for a complete description.

extra_rustc_flag = _extra_rustc_flag
# See @rules_rust//rust/private:rustc.bzl for a complete description.

extra_rustc_flags = _extra_rustc_flags
# See @rules_rust//rust/private:rustc.bzl for a complete description.

extra_exec_rustc_flag = _extra_exec_rustc_flag
# See @rules_rust//rust/private:rustc.bzl for a complete description.

extra_exec_rustc_flags = _extra_exec_rustc_flags
# See @rules_rust//rust/private:rustc.bzl for a complete description.

per_crate_rustc_flag = _per_crate_rustc_flag
# See @rules_rust//rust/private:rustc.bzl for a complete description.

rust_common = _rust_common
# See @rules_rust//rust/private:common.bzl for a complete description.

rust_analyzer_aspect = _rust_analyzer_aspect
# See @rules_rust//rust/private:rust_analyzer.bzl for a complete description.

rustfmt_aspect = _rustfmt_aspect
# See @rules_rust//rust/private:rustfmt.bzl for a complete description.

rustfmt_test = _rustfmt_test
# See @rules_rust//rust/private:rustfmt.bzl for a complete description.

rust_stdlib_filegroup = _rust_stdlib_filegroup
# See @rules_rust//rust:toolchain.bzl for a complete description.

no_std = _no_std
