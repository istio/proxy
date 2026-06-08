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

"""Module containing definitions of all Rust providers."""

CrateInfo = provider(
    doc = "A provider containing general Crate information.",
    fields = {
        "aliases": "Dict[Target, String]: Renamed and aliased crates",
        "cfgs": (
            "List[str]: The set of enabled cfgs for this crate. Note that this field is populated only " +
            "when @rules_rust//rust/settings:collect_cfgs is set."
        ),
        "compile_data": "depset[File]: Compile data required by this crate.",
        "compile_data_targets": "depset[Label]: Compile data targets required by this crate.",
        "data": "depset[File]: Compile data required by crates that use the current crate as a proc-macro.",
        "deps": "depset[DepVariantInfo]: This crate's (rust or cc) dependencies' providers.",
        "edition": "str: The edition of this crate.",
        "is_test": "bool: If the crate is being compiled in a test context",
        "metadata": "File: The output from rustc from producing the output file. It is optional.",
        "metadata_supports_pipelining": "bool: If the metadata in 'metadata' (if present) is " +
                                        "usable for pipelined compilation.",
        "name": "str: The name of this crate.",
        "output": "File: The output File that will be produced, depends on crate type.",
        "owner": "Label: The label of the target that produced this CrateInfo",
        "proc_macro_deps": "depset[DepVariantInfo]: This crate's rust proc_macro dependencies' providers.",
        "root": "File: The source File entrypoint to this crate, eg. lib.rs",
        "rustc_env": "Dict[String, String]: Additional `\"key\": \"value\"` environment variables to set for rustc.",
        "rustc_env_files": "[File]: Files containing additional environment variables to set for rustc.",
        "rustc_output": "File: The output from rustc from producing the output file. It is optional.",
        "rustc_rmeta_output": "File: The rmeta file produced for this crate. It is optional.",
        "srcs": "depset[File]: All source Files that are part of the crate.",
        "std_dylib": "File: libstd.so file",
        "type": (
            "str: The type of this crate " +
            "(see [rustc --crate-type](https://doc.rust-lang.org/rustc/command-line-arguments.html#--crate-type-a-list-of-types-of-crates-for-the-compiler-to-emit))."
        ),
        "wrapped_crate_type": (
            "str, optional: The original crate type for targets generated using a previously defined " +
            "crate (typically tests using the `rust_test::crate` attribute)"
        ),
    },
)

DepInfo = provider(
    doc = "A provider containing information about a Crate's dependencies.",
    fields = {
        "dep_env": "File: File with environment variables direct dependencies build scripts rely upon.",
        "direct_crates": "depset[AliasableDepInfo]",
        "link_search_path_files": "depset[File]: All transitive files containing search paths to pass to the linker",
        "transitive_build_infos": "depset[BuildInfo]",
        "transitive_crate_outputs": "depset[File]: All transitive crate outputs.",
        "transitive_crates": "depset[CrateInfo]",
        "transitive_data": "depset[File]: Data of all transitive non-macro dependencies.",
        "transitive_metadata_outputs": "depset[File]: All transitive metadata dependencies (.rmeta, for crates that provide them) and all transitive object dependencies (.rlib) for crates that don't provide metadata.",
        "transitive_noncrates": "depset[LinkerInput]: All transitive dependencies that aren't crates.",
        "transitive_proc_macro_data": "depset[File]: Data of all transitive proc-macro dependencies, and non-macro dependencies of those macros.",
    },
)

CrateGroupInfo = provider(
    doc = "A provider containing a group of crates.",
    fields = {
        "dep_variant_infos": "depset[DepVariantInfo]: Dependency information from all crates in the group.",
    },
)

BuildInfo = provider(
    doc = "A provider containing `rustc` build settings for a given Crate.",
    fields = {
        "compile_data": "Depset[File]: Compile data provided by the build script that was not copied into `out_dir`.",
        "dep_env": "Optional[File]: extra build script environment variables to be set to direct dependencies.",
        "flags": "Optional[File]: file containing additional flags to pass to rustc",
        "link_search_paths": "Optional[File]: file containing search paths to pass to rustc and linker",
        "linker_flags": "Optional[File]: file containing flags to pass to the linker invoked by rustc or cc_common.link",
        "out_dir": "Optional[File]: directory containing the result of a build script",
        "rustc_env": "Optional[File]: file containing additional environment variables to set for rustc.",
    },
)

DepVariantInfo = provider(
    doc = "A wrapper provider for a dependency of a crate. The dependency can be a Rust " +
          "dependency, in which case the `crate_info` and `dep_info` fields will be populated, " +
          "a Rust build script dependency, in which case `build_info` will be populated, a C/C++" +
          "dependency, in which case `cc_info` will be populated, or a Rust crate group, in which" +
          "case `crate_group_info` will be populated.",
    fields = {
        "build_info": "BuildInfo: The BuildInfo of a Rust dependency",
        "cc_info": "CcInfo: The CcInfo of a C/C++ dependency",
        "crate_group_info": "CrateGroupInfo: The CrateGroupInfo of a Rust crate group dependency",
        "crate_info": "CrateInfo: The CrateInfo of a Rust dependency",
        "dep_info": "DepInfo: The DepInfo of a Rust dependency",
    },
)

AlwaysEnableMetadataOutputGroupsInfo = provider(
    doc = (
        "Enable the 'metadata' and 'rustc_rmeta_output' groups for all targets, " +
        "even if not a library or if pipelining is disabled"
    ),
    fields = {
        "always_enable_metadata_output_groups": ("bool: Whether or not to always enable " +
                                                 "metadata-related output groups"),
    },
)

RustcOutputDiagnosticsInfo = provider(
    doc = (
        "Save json diagnostics from rustc. Json diagnostics are able to be " +
        "consumed by tools such as rust-analyzer to provide IDE integration"
    ),
    fields = {
        "rustc_output_diagnostics": "bool: Whether or not to output diagnostics",
    },
)

StdLibInfo = provider(
    doc = (
        "A collection of files either found within the `rust-stdlib` artifact or " +
        "generated based on existing files."
    ),
    fields = {
        "alloc_files": "List[File]: `.a` files related to the `alloc` module.",
        "between_alloc_and_core_files": "List[File]: `.a` files related to the `compiler_builtins` module.",
        "between_core_and_std_files": "List[File]: `.a` files related to all modules except `adler`, `alloc`, `compiler_builtins`, `core`, and `std`.",
        "core_files": "List[File]: `.a` files related to the `core` and `adler` modules",
        "dot_a_files": "Depset[File]: Generated `.a` files",
        "memchr_files": "Depset[File]: `.a` files associated with the `memchr` module.",
        "panic_files": "Depset[File]: `.a` files associated with `panic_unwind` and `panic_abort`.",
        "self_contained_files": "List[File]: All `.o` files from the `self-contained` directory.",
        "srcs": "List[Target]: All targets from the original `srcs` attribute.",
        "std_dylib": "File: libstd.so file",
        "std_files": "Depset[File]: `.a` files associated with the `std` module.",
        "std_rlibs": "List[File]: All `.rlib` files",
        "test_files": "Depset[File]: `.a` files associated with the `test` module.",
    },
)

CaptureClippyOutputInfo = provider(
    doc = "Value of the `capture_clippy_output` build setting",
    fields = {"capture_output": "Value of the `capture_clippy_output` build setting"},
)

ClippyOutputDiagnosticsInfo = provider(
    doc = "Value of the `clippy_output_diagnostics` build setting",
    fields = {"output_diagnostics": "Value of the `clippy_output_diagnostics` build setting"},
)

ClippyInfo = provider(
    doc = "Provides information on a clippy run.",
    fields = {
        "output": "File with the clippy output.",
    },
)

TestCrateInfo = provider(
    doc = "A wrapper around a CrateInfo. " +
          "Certain rule types, like rust_static_library and rust_shared_library " +
          "are not intended for consumption by other Rust targets, and should not " +
          "provide a CrateInfo. However, one should still be able to write a rust_test " +
          "for them. Thus, we create a CrateInfo, but do not advertise it as such, " +
          "but rather through this provider, that rust_test understands.",
    fields = {
        "crate": "CrateInfo: The underlying CrateInfo of the dependency",
    },
)

RustAnalyzerInfo = provider(
    doc = "RustAnalyzerInfo holds rust crate metadata for targets",
    fields = {
        "aliases": "Dict[RustAnalyzerInfo, String]: Replacement names these targets should be known as in Rust code",
        "build_info": "BuildInfo: build info for this crate if present",
        "build_info_out_dirs": "Depset[File]: transitive closure of build script out dirs",
        "cfgs": "List[String]: features or other compilation `--cfg` settings",
        "crate": "CrateInfo: Crate information.",
        "crate_specs": "Depset[File]: transitive closure of crate spec files",
        "deps": "List[RustAnalyzerInfo]: direct dependencies",
        "env": "Dict[String: String]: Environment variables, used for the `env!` macro",
        "proc_macro_dylib": "File: if this is a proc-macro target, the shared library output",
        "proc_macro_dylibs": "Depset[File]: transitive closure of proc-macro shared library files",
    },
)

RustAnalyzerGroupInfo = provider(
    doc = "RustAnalyzerGroupInfo holds multiple RustAnalyzerInfos",
    fields = {
        "deps": "List[RustAnalyzerInfo]: direct dependencies",
    },
)

LintsInfo = provider(
    doc = "LintsInfo holds the 'allow', 'warn', etc. config for rustc, clippy, and rustdoc lints.",
    fields = {
        "clippy_lint_files": "List[File]: files with rustc args for clippy targets.",
        "clippy_lint_flags": "List[String]: rustc flags to specify when building clippy targets.",
        "rustc_lint_files": "List[File]: list of files with rustc flags to specify when building rust_* targets.",
        "rustc_lint_flags": "List[String]: rustc flags to specify when building rust_* targets.",
        "rustdoc_lint_files": "List[File]: files with rustc args for rustdoc target.",
        "rustdoc_lint_flags": "List[String]: rustc flags to specify when building rust_doc targets.",
    },
)

AllocatorLibrariesInfo = provider(
    doc = "AllocatorLibrariesInfo provides allocator libraries for linking rust code with a non-rust linker.",
    fields = {
        "allocator_library": "Optional[CcInfo]: used when the default rust allocator is used",
        "global_allocator_library": "Optional[CcInfo]: used when a global rust allocator is used",
        "libstd_and_allocator_ccinfo": "Optional[CcInfo]: used when the default rust allocator is used",
        "libstd_and_global_allocator_ccinfo": "Optional[CcInfo]: used when a global rust allocator is used",
        "nostd_and_global_allocator_ccinfo": "Optional[CcInfo]: used when nostd with a global rust allocator is used",
    },
)

AllocatorLibrariesImplInfo = provider(
    doc = "AllocatorLibrariesImplInfo provides the rust-generated linker input for linking rust code with a non-rust linker.",
    fields = {
        "static_archive": "Optional[File]: the allocator library archive (typically .a file).",
    },
)
