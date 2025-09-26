<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Providers

* [CrateInfo](#CrateInfo)
* [DepInfo](#DepInfo)
* [StdLibInfo](#StdLibInfo)

<a id="CrateInfo"></a>

## CrateInfo

<pre>
CrateInfo(<a href="#CrateInfo-aliases">aliases</a>, <a href="#CrateInfo-compile_data">compile_data</a>, <a href="#CrateInfo-compile_data_targets">compile_data_targets</a>, <a href="#CrateInfo-data">data</a>, <a href="#CrateInfo-deps">deps</a>, <a href="#CrateInfo-edition">edition</a>, <a href="#CrateInfo-is_test">is_test</a>, <a href="#CrateInfo-metadata">metadata</a>, <a href="#CrateInfo-name">name</a>,
          <a href="#CrateInfo-output">output</a>, <a href="#CrateInfo-owner">owner</a>, <a href="#CrateInfo-proc_macro_deps">proc_macro_deps</a>, <a href="#CrateInfo-root">root</a>, <a href="#CrateInfo-rustc_env">rustc_env</a>, <a href="#CrateInfo-rustc_env_files">rustc_env_files</a>, <a href="#CrateInfo-rustc_output">rustc_output</a>,
          <a href="#CrateInfo-rustc_rmeta_output">rustc_rmeta_output</a>, <a href="#CrateInfo-srcs">srcs</a>, <a href="#CrateInfo-std_dylib">std_dylib</a>, <a href="#CrateInfo-type">type</a>, <a href="#CrateInfo-wrapped_crate_type">wrapped_crate_type</a>)
</pre>

A provider containing general Crate information.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="CrateInfo-aliases"></a>aliases |  Dict[Label, String]: Renamed and aliased crates    |
| <a id="CrateInfo-compile_data"></a>compile_data |  depset[File]: Compile data required by this crate.    |
| <a id="CrateInfo-compile_data_targets"></a>compile_data_targets |  depset[Label]: Compile data targets required by this crate.    |
| <a id="CrateInfo-data"></a>data |  depset[File]: Compile data required by crates that use the current crate as a proc-macro.    |
| <a id="CrateInfo-deps"></a>deps |  depset[DepVariantInfo]: This crate's (rust or cc) dependencies' providers.    |
| <a id="CrateInfo-edition"></a>edition |  str: The edition of this crate.    |
| <a id="CrateInfo-is_test"></a>is_test |  bool: If the crate is being compiled in a test context    |
| <a id="CrateInfo-metadata"></a>metadata |  File: The output from rustc from producing the output file. It is optional.    |
| <a id="CrateInfo-name"></a>name |  str: The name of this crate.    |
| <a id="CrateInfo-output"></a>output |  File: The output File that will be produced, depends on crate type.    |
| <a id="CrateInfo-owner"></a>owner |  Label: The label of the target that produced this CrateInfo    |
| <a id="CrateInfo-proc_macro_deps"></a>proc_macro_deps |  depset[DepVariantInfo]: This crate's rust proc_macro dependencies' providers.    |
| <a id="CrateInfo-root"></a>root |  File: The source File entrypoint to this crate, eg. lib.rs    |
| <a id="CrateInfo-rustc_env"></a>rustc_env |  Dict[String, String]: Additional `"key": "value"` environment variables to set for rustc.    |
| <a id="CrateInfo-rustc_env_files"></a>rustc_env_files |  [File]: Files containing additional environment variables to set for rustc.    |
| <a id="CrateInfo-rustc_output"></a>rustc_output |  File: The output from rustc from producing the output file. It is optional.    |
| <a id="CrateInfo-rustc_rmeta_output"></a>rustc_rmeta_output |  File: The rmeta file produced for this crate. It is optional.    |
| <a id="CrateInfo-srcs"></a>srcs |  depset[File]: All source Files that are part of the crate.    |
| <a id="CrateInfo-std_dylib"></a>std_dylib |  File: libstd.so file    |
| <a id="CrateInfo-type"></a>type |  str: The type of this crate (see [rustc --crate-type](https://doc.rust-lang.org/rustc/command-line-arguments.html#--crate-type-a-list-of-types-of-crates-for-the-compiler-to-emit)).    |
| <a id="CrateInfo-wrapped_crate_type"></a>wrapped_crate_type |  str, optional: The original crate type for targets generated using a previously defined crate (typically tests using the `rust_test::crate` attribute)    |


<a id="DepInfo"></a>

## DepInfo

<pre>
DepInfo(<a href="#DepInfo-dep_env">dep_env</a>, <a href="#DepInfo-direct_crates">direct_crates</a>, <a href="#DepInfo-link_search_path_files">link_search_path_files</a>, <a href="#DepInfo-transitive_build_infos">transitive_build_infos</a>,
        <a href="#DepInfo-transitive_crate_outputs">transitive_crate_outputs</a>, <a href="#DepInfo-transitive_crates">transitive_crates</a>, <a href="#DepInfo-transitive_data">transitive_data</a>, <a href="#DepInfo-transitive_metadata_outputs">transitive_metadata_outputs</a>,
        <a href="#DepInfo-transitive_noncrates">transitive_noncrates</a>, <a href="#DepInfo-transitive_proc_macro_data">transitive_proc_macro_data</a>)
</pre>

A provider containing information about a Crate's dependencies.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="DepInfo-dep_env"></a>dep_env |  File: File with environment variables direct dependencies build scripts rely upon.    |
| <a id="DepInfo-direct_crates"></a>direct_crates |  depset[AliasableDepInfo]    |
| <a id="DepInfo-link_search_path_files"></a>link_search_path_files |  depset[File]: All transitive files containing search paths to pass to the linker    |
| <a id="DepInfo-transitive_build_infos"></a>transitive_build_infos |  depset[BuildInfo]    |
| <a id="DepInfo-transitive_crate_outputs"></a>transitive_crate_outputs |  depset[File]: All transitive crate outputs.    |
| <a id="DepInfo-transitive_crates"></a>transitive_crates |  depset[CrateInfo]    |
| <a id="DepInfo-transitive_data"></a>transitive_data |  depset[File]: Data of all transitive non-macro dependencies.    |
| <a id="DepInfo-transitive_metadata_outputs"></a>transitive_metadata_outputs |  depset[File]: All transitive metadata dependencies (.rmeta, for crates that provide them) and all transitive object dependencies (.rlib) for crates that don't provide metadata.    |
| <a id="DepInfo-transitive_noncrates"></a>transitive_noncrates |  depset[LinkerInput]: All transitive dependencies that aren't crates.    |
| <a id="DepInfo-transitive_proc_macro_data"></a>transitive_proc_macro_data |  depset[File]: Data of all transitive proc-macro dependencies, and non-macro dependencies of those macros.    |


<a id="StdLibInfo"></a>

## StdLibInfo

<pre>
StdLibInfo(<a href="#StdLibInfo-alloc_files">alloc_files</a>, <a href="#StdLibInfo-between_alloc_and_core_files">between_alloc_and_core_files</a>, <a href="#StdLibInfo-between_core_and_std_files">between_core_and_std_files</a>, <a href="#StdLibInfo-core_files">core_files</a>,
           <a href="#StdLibInfo-dot_a_files">dot_a_files</a>, <a href="#StdLibInfo-memchr_files">memchr_files</a>, <a href="#StdLibInfo-panic_files">panic_files</a>, <a href="#StdLibInfo-self_contained_files">self_contained_files</a>, <a href="#StdLibInfo-srcs">srcs</a>, <a href="#StdLibInfo-std_dylib">std_dylib</a>, <a href="#StdLibInfo-std_files">std_files</a>,
           <a href="#StdLibInfo-std_rlibs">std_rlibs</a>, <a href="#StdLibInfo-test_files">test_files</a>)
</pre>

A collection of files either found within the `rust-stdlib` artifact or generated based on existing files.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="StdLibInfo-alloc_files"></a>alloc_files |  List[File]: `.a` files related to the `alloc` module.    |
| <a id="StdLibInfo-between_alloc_and_core_files"></a>between_alloc_and_core_files |  List[File]: `.a` files related to the `compiler_builtins` module.    |
| <a id="StdLibInfo-between_core_and_std_files"></a>between_core_and_std_files |  List[File]: `.a` files related to all modules except `adler`, `alloc`, `compiler_builtins`, `core`, and `std`.    |
| <a id="StdLibInfo-core_files"></a>core_files |  List[File]: `.a` files related to the `core` and `adler` modules    |
| <a id="StdLibInfo-dot_a_files"></a>dot_a_files |  Depset[File]: Generated `.a` files    |
| <a id="StdLibInfo-memchr_files"></a>memchr_files |  Depset[File]: `.a` files associated with the `memchr` module.    |
| <a id="StdLibInfo-panic_files"></a>panic_files |  Depset[File]: `.a` files associated with `panic_unwind` and `panic_abort`.    |
| <a id="StdLibInfo-self_contained_files"></a>self_contained_files |  List[File]: All `.o` files from the `self-contained` directory.    |
| <a id="StdLibInfo-srcs"></a>srcs |  List[Target]: All targets from the original `srcs` attribute.    |
| <a id="StdLibInfo-std_dylib"></a>std_dylib |  File: libstd.so file    |
| <a id="StdLibInfo-std_files"></a>std_files |  Depset[File]: `.a` files associated with the `std` module.    |
| <a id="StdLibInfo-std_rlibs"></a>std_rlibs |  List[File]: All `.rlib` files    |
| <a id="StdLibInfo-test_files"></a>test_files |  Depset[File]: `.a` files associated with the `test` module.    |


