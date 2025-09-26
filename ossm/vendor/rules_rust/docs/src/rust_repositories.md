<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Repositories

* [rules_rust_dependencies](#rules_rust_dependencies)
* [rust_analyzer_toolchain_repository](#rust_analyzer_toolchain_repository)
* [rust_register_toolchains](#rust_register_toolchains)
* [rust_repositories](#rust_repositories)
* [rust_repository_set](#rust_repository_set)
* [rust_stdlib_filegroup](#rust_stdlib_filegroup)
* [rust_toolchain_repository_proxy](#rust_toolchain_repository_proxy)
* [rust_toolchain_repository](#rust_toolchain_repository)
* [rust_toolchain_tools_repository](#rust_toolchain_tools_repository)
* [rust_toolchain](#rust_toolchain)

<a id="rust_stdlib_filegroup"></a>

## rust_stdlib_filegroup

<pre>
rust_stdlib_filegroup(<a href="#rust_stdlib_filegroup-name">name</a>, <a href="#rust_stdlib_filegroup-srcs">srcs</a>)
</pre>

A dedicated filegroup-like rule for Rust stdlib artifacts.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_stdlib_filegroup-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_stdlib_filegroup-srcs"></a>srcs |  The list of targets/files that are components of the rust-stdlib file group   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |


<a id="rust_toolchain"></a>

## rust_toolchain

<pre>
rust_toolchain(<a href="#rust_toolchain-name">name</a>, <a href="#rust_toolchain-allocator_library">allocator_library</a>, <a href="#rust_toolchain-binary_ext">binary_ext</a>, <a href="#rust_toolchain-cargo">cargo</a>, <a href="#rust_toolchain-cargo_clippy">cargo_clippy</a>, <a href="#rust_toolchain-clippy_driver">clippy_driver</a>, <a href="#rust_toolchain-debug_info">debug_info</a>,
               <a href="#rust_toolchain-default_edition">default_edition</a>, <a href="#rust_toolchain-dylib_ext">dylib_ext</a>, <a href="#rust_toolchain-env">env</a>, <a href="#rust_toolchain-exec_triple">exec_triple</a>, <a href="#rust_toolchain-experimental_link_std_dylib">experimental_link_std_dylib</a>,
               <a href="#rust_toolchain-experimental_use_cc_common_link">experimental_use_cc_common_link</a>, <a href="#rust_toolchain-extra_exec_rustc_flags">extra_exec_rustc_flags</a>, <a href="#rust_toolchain-extra_rustc_flags">extra_rustc_flags</a>,
               <a href="#rust_toolchain-extra_rustc_flags_for_crate_types">extra_rustc_flags_for_crate_types</a>, <a href="#rust_toolchain-global_allocator_library">global_allocator_library</a>, <a href="#rust_toolchain-llvm_cov">llvm_cov</a>, <a href="#rust_toolchain-llvm_profdata">llvm_profdata</a>,
               <a href="#rust_toolchain-llvm_tools">llvm_tools</a>, <a href="#rust_toolchain-opt_level">opt_level</a>, <a href="#rust_toolchain-per_crate_rustc_flags">per_crate_rustc_flags</a>, <a href="#rust_toolchain-rust_doc">rust_doc</a>, <a href="#rust_toolchain-rust_std">rust_std</a>, <a href="#rust_toolchain-rustc">rustc</a>, <a href="#rust_toolchain-rustc_lib">rustc_lib</a>,
               <a href="#rust_toolchain-rustfmt">rustfmt</a>, <a href="#rust_toolchain-staticlib_ext">staticlib_ext</a>, <a href="#rust_toolchain-stdlib_linkflags">stdlib_linkflags</a>, <a href="#rust_toolchain-strip_level">strip_level</a>, <a href="#rust_toolchain-target_json">target_json</a>, <a href="#rust_toolchain-target_triple">target_triple</a>)
</pre>

Declares a Rust toolchain for use.

This is for declaring a custom toolchain, eg. for configuring a particular version of rust or supporting a new platform.

Example:

Suppose the core rust team has ported the compiler to a new target CPU, called `cpuX`. This support can be used in Bazel by defining a new toolchain definition and declaration:

```python
load('@rules_rust//rust:toolchain.bzl', 'rust_toolchain')

rust_toolchain(
    name = "rust_cpuX_impl",
    binary_ext = "",
    dylib_ext = ".so",
    exec_triple = "cpuX-unknown-linux-gnu",
    rust_doc = "@rust_cpuX//:rustdoc",
    rust_std = "@rust_cpuX//:rust_std",
    rustc = "@rust_cpuX//:rustc",
    rustc_lib = "@rust_cpuX//:rustc_lib",
    staticlib_ext = ".a",
    stdlib_linkflags = ["-lpthread", "-ldl"],
    target_triple = "cpuX-unknown-linux-gnu",
)

toolchain(
    name = "rust_cpuX",
    exec_compatible_with = [
        "@platforms//cpu:cpuX",
        "@platforms//os:linux",
    ],
    target_compatible_with = [
        "@platforms//cpu:cpuX",
        "@platforms//os:linux",
    ],
    toolchain = ":rust_cpuX_impl",
)
```

Then, either add the label of the toolchain rule to `register_toolchains` in the WORKSPACE, or pass it to the `"--extra_toolchains"` flag for Bazel, and it will be used.

See `@rules_rust//rust:repositories.bzl` for examples of defining the `@rust_cpuX` repository with the actual binaries and libraries.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_toolchain-allocator_library"></a>allocator_library |  Target that provides allocator functions when rust_library targets are embedded in a cc_binary.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_rust//ffi/cc/allocator_library"`  |
| <a id="rust_toolchain-binary_ext"></a>binary_ext |  The extension for binaries created from rustc.   | String | required |  |
| <a id="rust_toolchain-cargo"></a>cargo |  The location of the `cargo` binary. Can be a direct source or a filegroup containing one item.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-cargo_clippy"></a>cargo_clippy |  The location of the `cargo_clippy` binary. Can be a direct source or a filegroup containing one item.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-clippy_driver"></a>clippy_driver |  The location of the `clippy-driver` binary. Can be a direct source or a filegroup containing one item.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-debug_info"></a>debug_info |  Rustc debug info levels per opt level   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{"dbg": "2", "fastbuild": "0", "opt": "0"}`  |
| <a id="rust_toolchain-default_edition"></a>default_edition |  The edition to use for rust_* rules that don't specify an edition. If absent, every rule is required to specify its `edition` attribute.   | String | optional |  `""`  |
| <a id="rust_toolchain-dylib_ext"></a>dylib_ext |  The extension for dynamic libraries created from rustc.   | String | required |  |
| <a id="rust_toolchain-env"></a>env |  Environment variables to set in actions.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_toolchain-exec_triple"></a>exec_triple |  The platform triple for the toolchains execution environment. For more details see: https://docs.bazel.build/versions/master/skylark/rules.html#configurations   | String | required |  |
| <a id="rust_toolchain-experimental_link_std_dylib"></a>experimental_link_std_dylib |  Label to a boolean build setting that controls whether whether to link libstd dynamically.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_rust//rust/settings:experimental_link_std_dylib"`  |
| <a id="rust_toolchain-experimental_use_cc_common_link"></a>experimental_use_cc_common_link |  Label to a boolean build setting that controls whether cc_common.link is used to link rust binaries.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_rust//rust/settings:experimental_use_cc_common_link"`  |
| <a id="rust_toolchain-extra_exec_rustc_flags"></a>extra_exec_rustc_flags |  Extra flags to pass to rustc in exec configuration   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain-extra_rustc_flags"></a>extra_rustc_flags |  Extra flags to pass to rustc in non-exec configuration. Subject to location expansion with respect to the srcs of the `rust_std` attribute.   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain-extra_rustc_flags_for_crate_types"></a>extra_rustc_flags_for_crate_types |  Extra flags to pass to rustc based on crate type   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> List of strings</a> | optional |  `{}`  |
| <a id="rust_toolchain-global_allocator_library"></a>global_allocator_library |  Target that provides allocator functions for when a global allocator is present.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_rust//ffi/cc/global_allocator_library"`  |
| <a id="rust_toolchain-llvm_cov"></a>llvm_cov |  The location of the `llvm-cov` binary. Can be a direct source or a filegroup containing one item. If None, rust code is not instrumented for coverage.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-llvm_profdata"></a>llvm_profdata |  The location of the `llvm-profdata` binary. Can be a direct source or a filegroup containing one item. If `llvm_cov` is None, this can be None as well and rust code is not instrumented for coverage.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-llvm_tools"></a>llvm_tools |  LLVM tools that are shipped with the Rust toolchain.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-opt_level"></a>opt_level |  Rustc optimization levels.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{"dbg": "0", "fastbuild": "0", "opt": "3"}`  |
| <a id="rust_toolchain-per_crate_rustc_flags"></a>per_crate_rustc_flags |  Extra flags to pass to rustc in non-exec configuration   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain-rust_doc"></a>rust_doc |  The location of the `rustdoc` binary. Can be a direct source or a filegroup containing one item.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_toolchain-rust_std"></a>rust_std |  The Rust standard library.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_toolchain-rustc"></a>rustc |  The location of the `rustc` binary. Can be a direct source or a filegroup containing one item.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_toolchain-rustc_lib"></a>rustc_lib |  The libraries used by rustc during compilation.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-rustfmt"></a>rustfmt |  **Deprecated**: Instead see [rustfmt_toolchain](#rustfmt_toolchain)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_toolchain-staticlib_ext"></a>staticlib_ext |  The extension for static libraries created from rustc.   | String | required |  |
| <a id="rust_toolchain-stdlib_linkflags"></a>stdlib_linkflags |  Additional linker flags to use when Rust standard library is linked by a C++ linker (rustc will deal with these automatically). Subject to location expansion with respect to the srcs of the `rust_std` attribute.   | List of strings | required |  |
| <a id="rust_toolchain-strip_level"></a>strip_level |  Rustc strip levels. For all potential options, see https://doc.rust-lang.org/rustc/codegen-options/index.html#strip   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{"dbg": "none", "fastbuild": "none", "opt": "debuginfo"}`  |
| <a id="rust_toolchain-target_json"></a>target_json |  Override the target_triple with a custom target specification. For more details see: https://doc.rust-lang.org/rustc/targets/custom.html   | String | optional |  `""`  |
| <a id="rust_toolchain-target_triple"></a>target_triple |  The platform triple for the toolchains target environment. For more details see: https://docs.bazel.build/versions/master/skylark/rules.html#configurations   | String | optional |  `""`  |


<a id="rules_rust_dependencies"></a>

## rules_rust_dependencies

<pre>
rules_rust_dependencies()
</pre>

Dependencies used in the implementation of `rules_rust`.



<a id="rust_analyzer_toolchain_repository"></a>

## rust_analyzer_toolchain_repository

<pre>
rust_analyzer_toolchain_repository(<a href="#rust_analyzer_toolchain_repository-name">name</a>, <a href="#rust_analyzer_toolchain_repository-version">version</a>, <a href="#rust_analyzer_toolchain_repository-exec_compatible_with">exec_compatible_with</a>, <a href="#rust_analyzer_toolchain_repository-target_compatible_with">target_compatible_with</a>,
                                   <a href="#rust_analyzer_toolchain_repository-sha256s">sha256s</a>, <a href="#rust_analyzer_toolchain_repository-urls">urls</a>, <a href="#rust_analyzer_toolchain_repository-auth">auth</a>, <a href="#rust_analyzer_toolchain_repository-netrc">netrc</a>, <a href="#rust_analyzer_toolchain_repository-auth_patterns">auth_patterns</a>)
</pre>

Assemble a remote rust_analyzer_toolchain target based on the given params.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_analyzer_toolchain_repository-name"></a>name |  The name of the toolchain proxy repository contianing the registerable toolchain.   |  none |
| <a id="rust_analyzer_toolchain_repository-version"></a>version |  The version of the tool among "nightly", "beta', or an exact version.   |  none |
| <a id="rust_analyzer_toolchain_repository-exec_compatible_with"></a>exec_compatible_with |  A list of constraints for the execution platform for this toolchain.   |  `[]` |
| <a id="rust_analyzer_toolchain_repository-target_compatible_with"></a>target_compatible_with |  A list of constraints for the target platform for this toolchain.   |  `[]` |
| <a id="rust_analyzer_toolchain_repository-sha256s"></a>sha256s |  A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.   |  `None` |
| <a id="rust_analyzer_toolchain_repository-urls"></a>urls |  A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format). Defaults to ['https://static.rust-lang.org/dist/{}.tar.xz']   |  `None` |
| <a id="rust_analyzer_toolchain_repository-auth"></a>auth |  Auth object compatible with repository_ctx.download to use when downloading files. See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.   |  `None` |
| <a id="rust_analyzer_toolchain_repository-netrc"></a>netrc |  .netrc file to use for authentication; mirrors the eponymous attribute from http_archive   |  `None` |
| <a id="rust_analyzer_toolchain_repository-auth_patterns"></a>auth_patterns |  Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive   |  `None` |

**RETURNS**

str: The name of a registerable rust_analyzer_toolchain.


<a id="rust_register_toolchains"></a>

## rust_register_toolchains

<pre>
rust_register_toolchains(<a href="#rust_register_toolchains-dev_components">dev_components</a>, <a href="#rust_register_toolchains-edition">edition</a>, <a href="#rust_register_toolchains-allocator_library">allocator_library</a>, <a href="#rust_register_toolchains-global_allocator_library">global_allocator_library</a>,
                         <a href="#rust_register_toolchains-register_toolchains">register_toolchains</a>, <a href="#rust_register_toolchains-rustfmt_version">rustfmt_version</a>, <a href="#rust_register_toolchains-rust_analyzer_version">rust_analyzer_version</a>, <a href="#rust_register_toolchains-sha256s">sha256s</a>,
                         <a href="#rust_register_toolchains-extra_target_triples">extra_target_triples</a>, <a href="#rust_register_toolchains-extra_rustc_flags">extra_rustc_flags</a>, <a href="#rust_register_toolchains-extra_exec_rustc_flags">extra_exec_rustc_flags</a>, <a href="#rust_register_toolchains-urls">urls</a>,
                         <a href="#rust_register_toolchains-versions">versions</a>, <a href="#rust_register_toolchains-aliases">aliases</a>, <a href="#rust_register_toolchains-hub_name">hub_name</a>, <a href="#rust_register_toolchains-compact_windows_names">compact_windows_names</a>, <a href="#rust_register_toolchains-toolchain_triples">toolchain_triples</a>,
                         <a href="#rust_register_toolchains-rustfmt_toolchain_triples">rustfmt_toolchain_triples</a>, <a href="#rust_register_toolchains-extra_toolchain_infos">extra_toolchain_infos</a>)
</pre>

Emits a default set of toolchains for Linux, MacOS, and Freebsd

Skip this macro and call the `rust_repository_set` macros directly if you need a compiler for     other hosts or for additional target triples.

The `sha256s` attribute represents a dict associating tool subdirectories to sha256 hashes. As an example:
```python
{
    "rust-1.46.0-x86_64-unknown-linux-gnu": "e3b98bc3440fe92817881933f9564389eccb396f5f431f33d48b979fa2fbdcf5",
    "rustfmt-1.4.12-x86_64-unknown-linux-gnu": "1894e76913303d66bf40885a601462844eec15fca9e76a6d13c390d7000d64b0",
    "rust-std-1.46.0-x86_64-unknown-linux-gnu": "ac04aef80423f612c0079829b504902de27a6997214eb58ab0765d02f7ec1dbc",
}
```
This would match for `exec_triple = "x86_64-unknown-linux-gnu"`.  If not specified, rules_rust pulls from a non-exhaustive     list of known checksums..

See `load_arbitrary_tool` in `@rules_rust//rust:repositories.bzl` for more details.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_register_toolchains-dev_components"></a>dev_components |  Whether to download the rustc-dev components (defaults to False). Requires version to be "nightly".   |  `False` |
| <a id="rust_register_toolchains-edition"></a>edition |  The rust edition to be used by default (2015, 2018, or 2021). If absent, every target is required to specify its `edition` attribute.   |  `None` |
| <a id="rust_register_toolchains-allocator_library"></a>allocator_library |  Target that provides allocator functions when rust_library targets are embedded in a cc_binary.   |  `None` |
| <a id="rust_register_toolchains-global_allocator_library"></a>global_allocator_library |  Target that provides allocator functions when global allocator is used with cc_common.link.   |  `None` |
| <a id="rust_register_toolchains-register_toolchains"></a>register_toolchains |  If true, repositories will be generated to produce and register `rust_toolchain` targets.   |  `True` |
| <a id="rust_register_toolchains-rustfmt_version"></a>rustfmt_version |  The version of rustfmt. If none is supplied and only a single version in `versions` is given, then this defaults to that version, otherwise will default to the default nightly version.   |  `None` |
| <a id="rust_register_toolchains-rust_analyzer_version"></a>rust_analyzer_version |  The version of Rustc to pair with rust-analyzer.   |  `None` |
| <a id="rust_register_toolchains-sha256s"></a>sha256s |  A dict associating tool subdirectories to sha256 hashes.   |  `None` |
| <a id="rust_register_toolchains-extra_target_triples"></a>extra_target_triples |  Additional rust-style targets that rust toolchains should support.   |  `["wasm32-unknown-unknown", "wasm32-wasip1"]` |
| <a id="rust_register_toolchains-extra_rustc_flags"></a>extra_rustc_flags |  Dictionary of target triples to list of extra flags to pass to rustc in non-exec configuration.   |  `None` |
| <a id="rust_register_toolchains-extra_exec_rustc_flags"></a>extra_exec_rustc_flags |  Extra flags to pass to rustc in exec configuration.   |  `None` |
| <a id="rust_register_toolchains-urls"></a>urls |  A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).   |  `["https://static.rust-lang.org/dist/{}.tar.xz"]` |
| <a id="rust_register_toolchains-versions"></a>versions |  A list of toolchain versions to download. This parameter only accepts one versions per channel. E.g. `["1.65.0", "nightly/2022-11-02", "beta/2020-12-30"]`.   |  `["1.83.0", "nightly/2024-11-28"]` |
| <a id="rust_register_toolchains-aliases"></a>aliases |  A mapping of "full" repository name to another name to use instead.   |  `{}` |
| <a id="rust_register_toolchains-hub_name"></a>hub_name |  The name of the bzlmod hub repository for toolchains.   |  `None` |
| <a id="rust_register_toolchains-compact_windows_names"></a>compact_windows_names |  Whether or not to produce compact repository names for windows toolchains. This is to avoid MAX_PATH issues.   |  `True` |
| <a id="rust_register_toolchains-toolchain_triples"></a>toolchain_triples |  Mapping of rust target triple -> repository name to create.   |  `{"aarch64-apple-darwin": "rust_darwin_aarch64", "aarch64-pc-windows-msvc": "rust_windows_aarch64", "aarch64-unknown-linux-gnu": "rust_linux_aarch64", "s390x-unknown-linux-gnu": "rust_linux_s390x", "x86_64-apple-darwin": "rust_darwin_x86_64", "x86_64-pc-windows-msvc": "rust_windows_x86_64", "x86_64-unknown-freebsd": "rust_freebsd_x86_64", "x86_64-unknown-linux-gnu": "rust_linux_x86_64"}` |
| <a id="rust_register_toolchains-rustfmt_toolchain_triples"></a>rustfmt_toolchain_triples |  Like toolchain_triples, but for rustfmt toolchains.   |  `{"aarch64-apple-darwin": "rust_darwin_aarch64", "aarch64-pc-windows-msvc": "rust_windows_aarch64", "aarch64-unknown-linux-gnu": "rust_linux_aarch64", "s390x-unknown-linux-gnu": "rust_linux_s390x", "x86_64-apple-darwin": "rust_darwin_x86_64", "x86_64-pc-windows-msvc": "rust_windows_x86_64", "x86_64-unknown-freebsd": "rust_freebsd_x86_64", "x86_64-unknown-linux-gnu": "rust_linux_x86_64"}` |
| <a id="rust_register_toolchains-extra_toolchain_infos"></a>extra_toolchain_infos |  (dict[str, dict], optional): Mapping of information about extra toolchains which were created outside of this call, which should be added to the hub repo.   |  `None` |


<a id="rust_repositories"></a>

## rust_repositories

<pre>
rust_repositories(<a href="#rust_repositories-kwargs">kwargs</a>)
</pre>

**Deprecated**: Use [rules_rust_dependencies](#rules_rust_dependencies)     and [rust_register_toolchains](#rust_register_toolchains) directly.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_repositories-kwargs"></a>kwargs |  Keyword arguments for the `rust_register_toolchains` macro.   |  none |


<a id="rust_repository_set"></a>

## rust_repository_set

<pre>
rust_repository_set(<a href="#rust_repository_set-name">name</a>, <a href="#rust_repository_set-versions">versions</a>, <a href="#rust_repository_set-exec_triple">exec_triple</a>, <a href="#rust_repository_set-target_settings">target_settings</a>, <a href="#rust_repository_set-allocator_library">allocator_library</a>,
                    <a href="#rust_repository_set-global_allocator_library">global_allocator_library</a>, <a href="#rust_repository_set-extra_target_triples">extra_target_triples</a>, <a href="#rust_repository_set-rustfmt_version">rustfmt_version</a>, <a href="#rust_repository_set-edition">edition</a>,
                    <a href="#rust_repository_set-dev_components">dev_components</a>, <a href="#rust_repository_set-extra_rustc_flags">extra_rustc_flags</a>, <a href="#rust_repository_set-extra_exec_rustc_flags">extra_exec_rustc_flags</a>, <a href="#rust_repository_set-opt_level">opt_level</a>, <a href="#rust_repository_set-sha256s">sha256s</a>,
                    <a href="#rust_repository_set-urls">urls</a>, <a href="#rust_repository_set-auth">auth</a>, <a href="#rust_repository_set-netrc">netrc</a>, <a href="#rust_repository_set-auth_patterns">auth_patterns</a>, <a href="#rust_repository_set-register_toolchain">register_toolchain</a>, <a href="#rust_repository_set-exec_compatible_with">exec_compatible_with</a>,
                    <a href="#rust_repository_set-default_target_compatible_with">default_target_compatible_with</a>, <a href="#rust_repository_set-aliases">aliases</a>, <a href="#rust_repository_set-compact_windows_names">compact_windows_names</a>)
</pre>

Assembles a remote repository for the given toolchain params, produces a proxy repository     to contain the toolchain declaration, and registers the toolchains.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_repository_set-name"></a>name |  The name of the generated repository   |  none |
| <a id="rust_repository_set-versions"></a>versions |  A list of toolchain versions to download. This paramter only accepts one versions per channel. E.g. `["1.65.0", "nightly/2022-11-02", "beta/2020-12-30"]`.   |  none |
| <a id="rust_repository_set-exec_triple"></a>exec_triple |  The Rust-style target that this compiler runs on   |  none |
| <a id="rust_repository_set-target_settings"></a>target_settings |  A list of config_settings that must be satisfied by the target configuration in order for this set of toolchains to be selected during toolchain resolution.   |  `[]` |
| <a id="rust_repository_set-allocator_library"></a>allocator_library |  Target that provides allocator functions when rust_library targets are embedded in a cc_binary.   |  `None` |
| <a id="rust_repository_set-global_allocator_library"></a>global_allocator_library |  Target that provides allocator functions a global allocator is used with cc_common.link.   |  `None` |
| <a id="rust_repository_set-extra_target_triples"></a>extra_target_triples |  Additional rust-style targets that this set of toolchains should support. If a map, values should be (optional) target_compatible_with lists for that particular target triple.   |  `{}` |
| <a id="rust_repository_set-rustfmt_version"></a>rustfmt_version |  The version of rustfmt to be associated with the toolchain.   |  `None` |
| <a id="rust_repository_set-edition"></a>edition |  The rust edition to be used by default (2015, 2018, or 2021). If absent, every rule is required to specify its `edition` attribute.   |  `None` |
| <a id="rust_repository_set-dev_components"></a>dev_components |  Whether to download the rustc-dev components. Requires version to be "nightly".   |  `False` |
| <a id="rust_repository_set-extra_rustc_flags"></a>extra_rustc_flags |  Dictionary of target triples to list of extra flags to pass to rustc in non-exec configuration.   |  `None` |
| <a id="rust_repository_set-extra_exec_rustc_flags"></a>extra_exec_rustc_flags |  Extra flags to pass to rustc in exec configuration.   |  `None` |
| <a id="rust_repository_set-opt_level"></a>opt_level |  Dictionary of target triples to optimiztion config.   |  `None` |
| <a id="rust_repository_set-sha256s"></a>sha256s |  A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.   |  `None` |
| <a id="rust_repository_set-urls"></a>urls |  A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).   |  `["https://static.rust-lang.org/dist/{}.tar.xz"]` |
| <a id="rust_repository_set-auth"></a>auth |  Auth object compatible with repository_ctx.download to use when downloading files. See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.   |  `None` |
| <a id="rust_repository_set-netrc"></a>netrc |  .netrc file to use for authentication; mirrors the eponymous attribute from http_archive   |  `None` |
| <a id="rust_repository_set-auth_patterns"></a>auth_patterns |  Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive   |  `None` |
| <a id="rust_repository_set-register_toolchain"></a>register_toolchain |  If True, the generated `rust_toolchain` target will become a registered toolchain.   |  `True` |
| <a id="rust_repository_set-exec_compatible_with"></a>exec_compatible_with |  A list of constraints for the execution platform for this toolchain.   |  `None` |
| <a id="rust_repository_set-default_target_compatible_with"></a>default_target_compatible_with |  A list of constraints for the target platform for this toolchain when the exec platform is the same as the target platform.   |  `None` |
| <a id="rust_repository_set-aliases"></a>aliases |  Replacement names to use for toolchains created by this macro.   |  `{}` |
| <a id="rust_repository_set-compact_windows_names"></a>compact_windows_names |  Whether or not to produce compact repository names for windows toolchains. This is to avoid MAX_PATH issues.   |  `True` |

**RETURNS**

dict[str, dict]: A dict of informations about all generated toolchains.


<a id="rust_toolchain_repository"></a>

## rust_toolchain_repository

<pre>
rust_toolchain_repository(<a href="#rust_toolchain_repository-name">name</a>, <a href="#rust_toolchain_repository-version">version</a>, <a href="#rust_toolchain_repository-exec_triple">exec_triple</a>, <a href="#rust_toolchain_repository-target_triple">target_triple</a>, <a href="#rust_toolchain_repository-exec_compatible_with">exec_compatible_with</a>,
                          <a href="#rust_toolchain_repository-target_compatible_with">target_compatible_with</a>, <a href="#rust_toolchain_repository-target_settings">target_settings</a>, <a href="#rust_toolchain_repository-channel">channel</a>, <a href="#rust_toolchain_repository-allocator_library">allocator_library</a>,
                          <a href="#rust_toolchain_repository-global_allocator_library">global_allocator_library</a>, <a href="#rust_toolchain_repository-rustfmt_version">rustfmt_version</a>, <a href="#rust_toolchain_repository-edition">edition</a>, <a href="#rust_toolchain_repository-dev_components">dev_components</a>,
                          <a href="#rust_toolchain_repository-extra_rustc_flags">extra_rustc_flags</a>, <a href="#rust_toolchain_repository-extra_exec_rustc_flags">extra_exec_rustc_flags</a>, <a href="#rust_toolchain_repository-opt_level">opt_level</a>, <a href="#rust_toolchain_repository-sha256s">sha256s</a>, <a href="#rust_toolchain_repository-urls">urls</a>, <a href="#rust_toolchain_repository-auth">auth</a>,
                          <a href="#rust_toolchain_repository-netrc">netrc</a>, <a href="#rust_toolchain_repository-auth_patterns">auth_patterns</a>)
</pre>

Assembles a remote repository for the given toolchain params, produces a proxy repository     to contain the toolchain declaration, and registers the toolchains.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_toolchain_repository-name"></a>name |  The name of the generated repository   |  none |
| <a id="rust_toolchain_repository-version"></a>version |  The version of the tool among "nightly", "beta", or an exact version.   |  none |
| <a id="rust_toolchain_repository-exec_triple"></a>exec_triple |  The Rust-style target that this compiler runs on.   |  none |
| <a id="rust_toolchain_repository-target_triple"></a>target_triple |  The Rust-style target to build for.   |  none |
| <a id="rust_toolchain_repository-exec_compatible_with"></a>exec_compatible_with |  A list of constraints for the execution platform for this toolchain.   |  `None` |
| <a id="rust_toolchain_repository-target_compatible_with"></a>target_compatible_with |  A list of constraints for the target platform for this toolchain.   |  `None` |
| <a id="rust_toolchain_repository-target_settings"></a>target_settings |  A list of config_settings that must be satisfied by the target configuration in order for this toolchain to be selected during toolchain resolution.   |  `[]` |
| <a id="rust_toolchain_repository-channel"></a>channel |  The channel of the Rust toolchain.   |  `None` |
| <a id="rust_toolchain_repository-allocator_library"></a>allocator_library |  Target that provides allocator functions when rust_library targets are embedded in a cc_binary.   |  `None` |
| <a id="rust_toolchain_repository-global_allocator_library"></a>global_allocator_library |  Target that provides allocator functions when a global allocator is used with cc_common.link.   |  `None` |
| <a id="rust_toolchain_repository-rustfmt_version"></a>rustfmt_version |  The version of rustfmt to be associated with the toolchain.   |  `None` |
| <a id="rust_toolchain_repository-edition"></a>edition |  The rust edition to be used by default (2015, 2018, or 2021). If absent, every rule is required to specify its `edition` attribute.   |  `None` |
| <a id="rust_toolchain_repository-dev_components"></a>dev_components |  Whether to download the rustc-dev components. Requires version to be "nightly". Defaults to False.   |  `False` |
| <a id="rust_toolchain_repository-extra_rustc_flags"></a>extra_rustc_flags |  Extra flags to pass to rustc in non-exec configuration.   |  `None` |
| <a id="rust_toolchain_repository-extra_exec_rustc_flags"></a>extra_exec_rustc_flags |  Extra flags to pass to rustc in exec configuration.   |  `None` |
| <a id="rust_toolchain_repository-opt_level"></a>opt_level |  Optimization level config for this toolchain.   |  `None` |
| <a id="rust_toolchain_repository-sha256s"></a>sha256s |  A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.   |  `None` |
| <a id="rust_toolchain_repository-urls"></a>urls |  A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format). Defaults to ['https://static.rust-lang.org/dist/{}.tar.xz']   |  `["https://static.rust-lang.org/dist/{}.tar.xz"]` |
| <a id="rust_toolchain_repository-auth"></a>auth |  Auth object compatible with repository_ctx.download to use when downloading files. See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.   |  `None` |
| <a id="rust_toolchain_repository-netrc"></a>netrc |  .netrc file to use for authentication; mirrors the eponymous attribute from http_archive   |  `None` |
| <a id="rust_toolchain_repository-auth_patterns"></a>auth_patterns |  A list of patterns to match against urls for which the auth object should be used.   |  `None` |

**RETURNS**

dict[str, str]: Information about the registerable toolchain created by this rule.


<a id="rust_toolchain_repository_proxy"></a>

## rust_toolchain_repository_proxy

<pre>
rust_toolchain_repository_proxy(<a href="#rust_toolchain_repository_proxy-name">name</a>, <a href="#rust_toolchain_repository_proxy-exec_compatible_with">exec_compatible_with</a>, <a href="#rust_toolchain_repository_proxy-repo_mapping">repo_mapping</a>, <a href="#rust_toolchain_repository_proxy-target_compatible_with">target_compatible_with</a>,
                                <a href="#rust_toolchain_repository_proxy-target_settings">target_settings</a>, <a href="#rust_toolchain_repository_proxy-toolchain">toolchain</a>, <a href="#rust_toolchain_repository_proxy-toolchain_type">toolchain_type</a>)
</pre>

Generates a toolchain-bearing repository that declares the toolchains from some other rust_toolchain_repository.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_toolchain_repository_proxy-name"></a>name |  A unique name for this repository.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_toolchain_repository_proxy-exec_compatible_with"></a>exec_compatible_with |  A list of constraints for the execution platform for this toolchain.   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_repository_proxy-repo_mapping"></a>repo_mapping |  In `WORKSPACE` context only: a dictionary from local repository name to global repository name. This allows controls over workspace dependency resolution for dependencies of this repository.<br><br>For example, an entry `"@foo": "@bar"` declares that, for any time this repository depends on `@foo` (such as a dependency on `@foo//some:target`, it should actually resolve that dependency within globally-declared `@bar` (`@bar//some:target`).<br><br>This attribute is _not_ supported in `MODULE.bazel` context (when invoking a repository rule inside a module extension's implementation function).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  |
| <a id="rust_toolchain_repository_proxy-target_compatible_with"></a>target_compatible_with |  A list of constraints for the target platform for this toolchain.   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_repository_proxy-target_settings"></a>target_settings |  A list of config_settings that must be satisfied by the target configuration in order for this toolchain to be selected during toolchain resolution.   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_repository_proxy-toolchain"></a>toolchain |  The name of the toolchain implementation target.   | String | required |  |
| <a id="rust_toolchain_repository_proxy-toolchain_type"></a>toolchain_type |  The toolchain type of the toolchain to declare   | String | required |  |


<a id="rust_toolchain_tools_repository"></a>

## rust_toolchain_tools_repository

<pre>
rust_toolchain_tools_repository(<a href="#rust_toolchain_tools_repository-name">name</a>, <a href="#rust_toolchain_tools_repository-allocator_library">allocator_library</a>, <a href="#rust_toolchain_tools_repository-auth">auth</a>, <a href="#rust_toolchain_tools_repository-auth_patterns">auth_patterns</a>, <a href="#rust_toolchain_tools_repository-dev_components">dev_components</a>,
                                <a href="#rust_toolchain_tools_repository-edition">edition</a>, <a href="#rust_toolchain_tools_repository-exec_triple">exec_triple</a>, <a href="#rust_toolchain_tools_repository-extra_exec_rustc_flags">extra_exec_rustc_flags</a>, <a href="#rust_toolchain_tools_repository-extra_rustc_flags">extra_rustc_flags</a>,
                                <a href="#rust_toolchain_tools_repository-global_allocator_library">global_allocator_library</a>, <a href="#rust_toolchain_tools_repository-netrc">netrc</a>, <a href="#rust_toolchain_tools_repository-opt_level">opt_level</a>, <a href="#rust_toolchain_tools_repository-repo_mapping">repo_mapping</a>,
                                <a href="#rust_toolchain_tools_repository-rustfmt_version">rustfmt_version</a>, <a href="#rust_toolchain_tools_repository-sha256s">sha256s</a>, <a href="#rust_toolchain_tools_repository-target_triple">target_triple</a>, <a href="#rust_toolchain_tools_repository-urls">urls</a>, <a href="#rust_toolchain_tools_repository-version">version</a>)
</pre>

Composes a single workspace containing the toolchain components for compiling on a given platform to a series of target platforms.

A given instance of this rule should be accompanied by a toolchain_repository_proxy invocation to declare its toolchains to Bazel; the indirection allows separating toolchain selection from toolchain fetching.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_toolchain_tools_repository-name"></a>name |  A unique name for this repository.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_toolchain_tools_repository-allocator_library"></a>allocator_library |  Target that provides allocator functions when rust_library targets are embedded in a cc_binary.   | String | optional |  `"@rules_rust//ffi/cc/allocator_library"`  |
| <a id="rust_toolchain_tools_repository-auth"></a>auth |  Auth object compatible with repository_ctx.download to use when downloading files. See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_toolchain_tools_repository-auth_patterns"></a>auth_patterns |  A list of patterns to match against urls for which the auth object should be used.   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_tools_repository-dev_components"></a>dev_components |  Whether to download the rustc-dev components (defaults to False). Requires version to be "nightly".   | Boolean | optional |  `False`  |
| <a id="rust_toolchain_tools_repository-edition"></a>edition |  The rust edition to be used by default (2015, 2018, or 2021). If absent, every rule is required to specify its `edition` attribute.   | String | optional |  `""`  |
| <a id="rust_toolchain_tools_repository-exec_triple"></a>exec_triple |  The Rust-style target that this compiler runs on   | String | required |  |
| <a id="rust_toolchain_tools_repository-extra_exec_rustc_flags"></a>extra_exec_rustc_flags |  Extra flags to pass to rustc in exec configuration   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_tools_repository-extra_rustc_flags"></a>extra_rustc_flags |  Extra flags to pass to rustc in non-exec configuration   | List of strings | optional |  `[]`  |
| <a id="rust_toolchain_tools_repository-global_allocator_library"></a>global_allocator_library |  Target that provides allocator functions when a global allocator is used with cc_common.link.   | String | optional |  `"@rules_rust//ffi/cc/global_allocator_library"`  |
| <a id="rust_toolchain_tools_repository-netrc"></a>netrc |  .netrc file to use for authentication; mirrors the eponymous attribute from http_archive   | String | optional |  `""`  |
| <a id="rust_toolchain_tools_repository-opt_level"></a>opt_level |  Rustc optimization levels. For more details see the documentation for `rust_toolchain.opt_level`.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_toolchain_tools_repository-repo_mapping"></a>repo_mapping |  In `WORKSPACE` context only: a dictionary from local repository name to global repository name. This allows controls over workspace dependency resolution for dependencies of this repository.<br><br>For example, an entry `"@foo": "@bar"` declares that, for any time this repository depends on `@foo` (such as a dependency on `@foo//some:target`, it should actually resolve that dependency within globally-declared `@bar` (`@bar//some:target`).<br><br>This attribute is _not_ supported in `MODULE.bazel` context (when invoking a repository rule inside a module extension's implementation function).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  |
| <a id="rust_toolchain_tools_repository-rustfmt_version"></a>rustfmt_version |  The version of the tool among "nightly", "beta", or an exact version.   | String | optional |  `""`  |
| <a id="rust_toolchain_tools_repository-sha256s"></a>sha256s |  A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_toolchain_tools_repository-target_triple"></a>target_triple |  The Rust-style target that this compiler builds for.   | String | required |  |
| <a id="rust_toolchain_tools_repository-urls"></a>urls |  A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).   | List of strings | optional |  `["https://static.rust-lang.org/dist/{}.tar.xz"]`  |
| <a id="rust_toolchain_tools_repository-version"></a>version |  The version of the tool among "nightly", "beta", or an exact version.   | String | required |  |


