<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust

* [rust_binary](#rust_binary)
* [rust_library](#rust_library)
* [rust_library_group](#rust_library_group)
* [rust_static_library](#rust_static_library)
* [rust_shared_library](#rust_shared_library)
* [rust_proc_macro](#rust_proc_macro)
* [rust_test](#rust_test)
* [rust_test_suite](#rust_test_suite)
* [error_format](#error_format)
* [extra_rustc_flag](#extra_rustc_flag)
* [extra_rustc_flags](#extra_rustc_flags)
* [capture_clippy_output](#capture_clippy_output)

<a id="capture_clippy_output"></a>

## capture_clippy_output

<pre>
capture_clippy_output(<a href="#capture_clippy_output-name">name</a>)
</pre>

Control whether to print clippy output or store it to a file, using the configured error_format.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="capture_clippy_output-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


<a id="error_format"></a>

## error_format

<pre>
error_format(<a href="#error_format-name">name</a>)
</pre>

Change the [--error-format](https://doc.rust-lang.org/rustc/command-line-arguments.html#option-error-format) flag from the command line with `--@rules_rust//rust/settings:error_format`. See rustc documentation for valid values.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="error_format-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


<a id="extra_rustc_flag"></a>

## extra_rustc_flag

<pre>
extra_rustc_flag(<a href="#extra_rustc_flag-name">name</a>)
</pre>

Add additional rustc_flag from the command line with `--@rules_rust//rust/settings:extra_rustc_flag`. Multiple uses are accumulated and appended after the extra_rustc_flags.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="extra_rustc_flag-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


<a id="extra_rustc_flags"></a>

## extra_rustc_flags

<pre>
extra_rustc_flags(<a href="#extra_rustc_flags-name">name</a>)
</pre>

Add additional rustc_flags from the command line with `--@rules_rust//rust/settings:extra_rustc_flags`. This flag should only be used for flags that need to be applied across the entire build. For options that apply to individual crates, use the rustc_flags attribute on the individual crate's rule instead. NOTE: These flags not applied to the exec configuration (proc-macros, cargo_build_script, etc); use `--@rules_rust//rust/settings:extra_exec_rustc_flags` to apply flags to the exec configuration.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="extra_rustc_flags-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


<a id="rust_binary"></a>

## rust_binary

<pre>
rust_binary(<a href="#rust_binary-name">name</a>, <a href="#rust_binary-deps">deps</a>, <a href="#rust_binary-srcs">srcs</a>, <a href="#rust_binary-data">data</a>, <a href="#rust_binary-aliases">aliases</a>, <a href="#rust_binary-alwayslink">alwayslink</a>, <a href="#rust_binary-binary_name">binary_name</a>, <a href="#rust_binary-compile_data">compile_data</a>, <a href="#rust_binary-crate_features">crate_features</a>,
            <a href="#rust_binary-crate_name">crate_name</a>, <a href="#rust_binary-crate_root">crate_root</a>, <a href="#rust_binary-crate_type">crate_type</a>, <a href="#rust_binary-edition">edition</a>, <a href="#rust_binary-env">env</a>, <a href="#rust_binary-experimental_use_cc_common_link">experimental_use_cc_common_link</a>,
            <a href="#rust_binary-linker_script">linker_script</a>, <a href="#rust_binary-malloc">malloc</a>, <a href="#rust_binary-out_binary">out_binary</a>, <a href="#rust_binary-platform">platform</a>, <a href="#rust_binary-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_binary-rustc_env">rustc_env</a>, <a href="#rust_binary-rustc_env_files">rustc_env_files</a>,
            <a href="#rust_binary-rustc_flags">rustc_flags</a>, <a href="#rust_binary-stamp">stamp</a>, <a href="#rust_binary-version">version</a>)
</pre>

Builds a Rust binary crate.

Example:

Suppose you have the following directory structure for a Rust project with a
library crate, `hello_lib`, and a binary crate, `hello_world` that uses the
`hello_lib` library:

```output
[workspace]/
    WORKSPACE
    hello_lib/
        BUILD
        src/
            lib.rs
    hello_world/
        BUILD
        src/
            main.rs
```

`hello_lib/src/lib.rs`:
```rust
pub struct Greeter {
    greeting: String,
}

impl Greeter {
    pub fn new(greeting: &str) -> Greeter {
        Greeter { greeting: greeting.to_string(), }
    }

    pub fn greet(&self, thing: &str) {
        println!("{} {}", &self.greeting, thing);
    }
}
```

`hello_lib/BUILD`:
```python
package(default_visibility = ["//visibility:public"])

load("@rules_rust//rust:defs.bzl", "rust_library")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)
```

`hello_world/src/main.rs`:
```rust
extern crate hello_lib;

fn main() {
    let hello = hello_lib::Greeter::new("Hello");
    hello.greet("world");
}
```

`hello_world/BUILD`:
```python
load("@rules_rust//rust:defs.bzl", "rust_binary")

rust_binary(
    name = "hello_world",
    srcs = ["src/main.rs"],
    deps = ["//hello_lib"],
)
```

Build and run `hello_world`:
```
$ bazel run //hello_world
INFO: Found 1 target...
Target //examples/rust/hello_world:hello_world up-to-date:
bazel-bin/examples/rust/hello_world/hello_world
INFO: Elapsed time: 1.308s, Critical Path: 1.22s

INFO: Running command line: bazel-bin/examples/rust/hello_world/hello_world
Hello world
```

On Windows, a PDB file containing debugging information is available under
the key `pdb_file` in `OutputGroupInfo`. Similarly on macOS, a dSYM folder
is available under the key `dsym_folder` in `OutputGroupInfo`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_binary-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_binary-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_binary-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_binary-binary_name"></a>binary_name |  Override the resulting binary file name. By default, the binary file will be named using the `name` attribute on this rule, however sometimes that is not deseriable.   | String | optional |  `""`  |
| <a id="rust_binary-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_binary-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_binary-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_binary-crate_type"></a>crate_type |  Crate type that will be passed to `rustc` to be used for building this crate.<br><br>This option is a temporary workaround and should be used only when building for WebAssembly targets (//rust/platform:wasi and //rust/platform:wasm).   | String | optional |  `"bin"`  |
| <a id="rust_binary-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_binary-env"></a>env |  Specifies additional environment variables to set when the target is executed by bazel run. Values are subject to `$(rootpath)`, `$(execpath)`, location, and ["Make variable"](https://docs.bazel.build/versions/master/be/make-variables.html) substitution.<br><br>Execpath returns absolute path, and in order to be able to construct the absolute path we need to wrap the test binary in a launcher. Using a launcher comes with complications, such as more complicated debugger attachment.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_binary-experimental_use_cc_common_link"></a>experimental_use_cc_common_link |  Whether to use cc_common.link to link rust binaries. Possible values: [-1, 0, 1]. -1 means use the value of the toolchain.experimental_use_cc_common_link boolean build setting to determine. 0 means do not use cc_common.link (use rustc instead). 1 means use cc_common.link.   | Integer | optional |  `-1`  |
| <a id="rust_binary-linker_script"></a>linker_script |  Link script to forward into linker via rustc options.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_binary-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Rust binaries linked with cc_common.link are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="rust_binary-out_binary"></a>out_binary |  Force a target, regardless of it's `crate_type`, to always mark the file as executable. This attribute is only used to support wasm targets but is expected to be removed following a resolution to https://github.com/bazelbuild/rules_rust/issues/771.   | Boolean | optional |  `False`  |
| <a id="rust_binary-platform"></a>platform |  Optional platform to transition the binary to.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_binary-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_binary-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_binary-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_binary-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `-1`  |
| <a id="rust_binary-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_library"></a>

## rust_library

<pre>
rust_library(<a href="#rust_library-name">name</a>, <a href="#rust_library-deps">deps</a>, <a href="#rust_library-srcs">srcs</a>, <a href="#rust_library-data">data</a>, <a href="#rust_library-aliases">aliases</a>, <a href="#rust_library-alwayslink">alwayslink</a>, <a href="#rust_library-compile_data">compile_data</a>, <a href="#rust_library-crate_features">crate_features</a>, <a href="#rust_library-crate_name">crate_name</a>,
             <a href="#rust_library-crate_root">crate_root</a>, <a href="#rust_library-disable_pipelining">disable_pipelining</a>, <a href="#rust_library-edition">edition</a>, <a href="#rust_library-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_library-rustc_env">rustc_env</a>, <a href="#rust_library-rustc_env_files">rustc_env_files</a>,
             <a href="#rust_library-rustc_flags">rustc_flags</a>, <a href="#rust_library-stamp">stamp</a>, <a href="#rust_library-version">version</a>)
</pre>

Builds a Rust library crate.

Example:

Suppose you have the following directory structure for a simple Rust library crate:

```output
[workspace]/
    WORKSPACE
    hello_lib/
        BUILD
        src/
            greeter.rs
            lib.rs
```

`hello_lib/src/greeter.rs`:
```rust
pub struct Greeter {
    greeting: String,
}

impl Greeter {
    pub fn new(greeting: &str) -> Greeter {
        Greeter { greeting: greeting.to_string(), }
    }

    pub fn greet(&self, thing: &str) {
        println!("{} {}", &self.greeting, thing);
    }
}
```

`hello_lib/src/lib.rs`:

```rust
pub mod greeter;
```

`hello_lib/BUILD`:
```python
package(default_visibility = ["//visibility:public"])

load("@rules_rust//rust:defs.bzl", "rust_library")

rust_library(
    name = "hello_lib",
    srcs = [
        "src/greeter.rs",
        "src/lib.rs",
    ],
)
```

Build the library:
```output
$ bazel build //hello_lib
INFO: Found 1 target...
Target //examples/rust/hello_lib:hello_lib up-to-date:
bazel-bin/examples/rust/hello_lib/libhello_lib.rlib
INFO: Elapsed time: 1.245s, Critical Path: 1.01s
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_library-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_library-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_library-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_library-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_library-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_library-disable_pipelining"></a>disable_pipelining |  Disables pipelining for this rule if it is globally enabled. This will cause this rule to not produce a `.rmeta` file and all the dependent crates will instead use the `.rlib` file.   | Boolean | optional |  `False`  |
| <a id="rust_library-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_library-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_library-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_library-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_library-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `0`  |
| <a id="rust_library-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_library_group"></a>

## rust_library_group

<pre>
rust_library_group(<a href="#rust_library_group-name">name</a>, <a href="#rust_library_group-deps">deps</a>)
</pre>

Functions as an alias for a set of dependencies.

Specifically, the following are equivalent:

```starlark
rust_library_group(
    name = "crate_group",
    deps = [
        ":crate1",
        ":crate2",
    ],
)

rust_library(
    name = "foobar",
    deps = [":crate_group"],
    ...
)
```

and

```starlark
rust_library(
    name = "foobar",
    deps = [
        ":crate1",
        ":crate2",
    ],
    ...
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_library_group-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_library_group-deps"></a>deps |  Other dependencies to forward through this crate group.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="rust_proc_macro"></a>

## rust_proc_macro

<pre>
rust_proc_macro(<a href="#rust_proc_macro-name">name</a>, <a href="#rust_proc_macro-deps">deps</a>, <a href="#rust_proc_macro-srcs">srcs</a>, <a href="#rust_proc_macro-data">data</a>, <a href="#rust_proc_macro-aliases">aliases</a>, <a href="#rust_proc_macro-alwayslink">alwayslink</a>, <a href="#rust_proc_macro-compile_data">compile_data</a>, <a href="#rust_proc_macro-crate_features">crate_features</a>,
                <a href="#rust_proc_macro-crate_name">crate_name</a>, <a href="#rust_proc_macro-crate_root">crate_root</a>, <a href="#rust_proc_macro-edition">edition</a>, <a href="#rust_proc_macro-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_proc_macro-rustc_env">rustc_env</a>, <a href="#rust_proc_macro-rustc_env_files">rustc_env_files</a>,
                <a href="#rust_proc_macro-rustc_flags">rustc_flags</a>, <a href="#rust_proc_macro-stamp">stamp</a>, <a href="#rust_proc_macro-version">version</a>)
</pre>

Builds a Rust proc-macro crate.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_proc_macro-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_proc_macro-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_proc_macro-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_proc_macro-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_proc_macro-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_proc_macro-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_proc_macro-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_proc_macro-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_proc_macro-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proc_macro-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_proc_macro-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `0`  |
| <a id="rust_proc_macro-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_shared_library"></a>

## rust_shared_library

<pre>
rust_shared_library(<a href="#rust_shared_library-name">name</a>, <a href="#rust_shared_library-deps">deps</a>, <a href="#rust_shared_library-srcs">srcs</a>, <a href="#rust_shared_library-data">data</a>, <a href="#rust_shared_library-aliases">aliases</a>, <a href="#rust_shared_library-alwayslink">alwayslink</a>, <a href="#rust_shared_library-compile_data">compile_data</a>, <a href="#rust_shared_library-crate_features">crate_features</a>,
                    <a href="#rust_shared_library-crate_name">crate_name</a>, <a href="#rust_shared_library-crate_root">crate_root</a>, <a href="#rust_shared_library-edition">edition</a>, <a href="#rust_shared_library-experimental_use_cc_common_link">experimental_use_cc_common_link</a>, <a href="#rust_shared_library-malloc">malloc</a>,
                    <a href="#rust_shared_library-platform">platform</a>, <a href="#rust_shared_library-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_shared_library-rustc_env">rustc_env</a>, <a href="#rust_shared_library-rustc_env_files">rustc_env_files</a>, <a href="#rust_shared_library-rustc_flags">rustc_flags</a>, <a href="#rust_shared_library-stamp">stamp</a>,
                    <a href="#rust_shared_library-version">version</a>)
</pre>

Builds a Rust shared library.

This shared library will contain all transitively reachable crates and native objects.
It is meant to be used when producing an artifact that is then consumed by some other build system
(for example to produce a shared library that Python program links against).

This rule provides CcInfo, so it can be used everywhere Bazel expects `rules_cc`.

When building the whole binary in Bazel, use `rust_library` instead.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_shared_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_shared_library-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_shared_library-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_shared_library-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_shared_library-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_shared_library-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_shared_library-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_shared_library-experimental_use_cc_common_link"></a>experimental_use_cc_common_link |  Whether to use cc_common.link to link rust binaries. Possible values: [-1, 0, 1]. -1 means use the value of the toolchain.experimental_use_cc_common_link boolean build setting to determine. 0 means do not use cc_common.link (use rustc instead). 1 means use cc_common.link.   | Integer | optional |  `-1`  |
| <a id="rust_shared_library-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Rust binaries linked with cc_common.link are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="rust_shared_library-platform"></a>platform |  Optional platform to transition the shared library to.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_shared_library-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_shared_library-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_shared_library-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_shared_library-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `0`  |
| <a id="rust_shared_library-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_static_library"></a>

## rust_static_library

<pre>
rust_static_library(<a href="#rust_static_library-name">name</a>, <a href="#rust_static_library-deps">deps</a>, <a href="#rust_static_library-srcs">srcs</a>, <a href="#rust_static_library-data">data</a>, <a href="#rust_static_library-aliases">aliases</a>, <a href="#rust_static_library-alwayslink">alwayslink</a>, <a href="#rust_static_library-compile_data">compile_data</a>, <a href="#rust_static_library-crate_features">crate_features</a>,
                    <a href="#rust_static_library-crate_name">crate_name</a>, <a href="#rust_static_library-crate_root">crate_root</a>, <a href="#rust_static_library-edition">edition</a>, <a href="#rust_static_library-platform">platform</a>, <a href="#rust_static_library-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_static_library-rustc_env">rustc_env</a>,
                    <a href="#rust_static_library-rustc_env_files">rustc_env_files</a>, <a href="#rust_static_library-rustc_flags">rustc_flags</a>, <a href="#rust_static_library-stamp">stamp</a>, <a href="#rust_static_library-version">version</a>)
</pre>

Builds a Rust static library.

This static library will contain all transitively reachable crates and native objects.
It is meant to be used when producing an artifact that is then consumed by some other build system
(for example to produce an archive that Python program links against).

This rule provides CcInfo, so it can be used everywhere Bazel expects `rules_cc`.

When building the whole binary in Bazel, use `rust_library` instead.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_static_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_static_library-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_static_library-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_static_library-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_static_library-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_static_library-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_static_library-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_static_library-platform"></a>platform |  Optional platform to transition the static library to.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_static_library-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_static_library-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_static_library-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_static_library-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `0`  |
| <a id="rust_static_library-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_test"></a>

## rust_test

<pre>
rust_test(<a href="#rust_test-name">name</a>, <a href="#rust_test-deps">deps</a>, <a href="#rust_test-srcs">srcs</a>, <a href="#rust_test-data">data</a>, <a href="#rust_test-aliases">aliases</a>, <a href="#rust_test-alwayslink">alwayslink</a>, <a href="#rust_test-compile_data">compile_data</a>, <a href="#rust_test-crate">crate</a>, <a href="#rust_test-crate_features">crate_features</a>,
          <a href="#rust_test-crate_name">crate_name</a>, <a href="#rust_test-crate_root">crate_root</a>, <a href="#rust_test-edition">edition</a>, <a href="#rust_test-env">env</a>, <a href="#rust_test-env_inherit">env_inherit</a>, <a href="#rust_test-experimental_use_cc_common_link">experimental_use_cc_common_link</a>, <a href="#rust_test-malloc">malloc</a>,
          <a href="#rust_test-platform">platform</a>, <a href="#rust_test-proc_macro_deps">proc_macro_deps</a>, <a href="#rust_test-rustc_env">rustc_env</a>, <a href="#rust_test-rustc_env_files">rustc_env_files</a>, <a href="#rust_test-rustc_flags">rustc_flags</a>, <a href="#rust_test-stamp">stamp</a>,
          <a href="#rust_test-use_libtest_harness">use_libtest_harness</a>, <a href="#rust_test-version">version</a>)
</pre>

Builds a Rust test crate.

Examples:

Suppose you have the following directory structure for a Rust library crate         with unit test code in the library sources:

```output
[workspace]/
    WORKSPACE
    hello_lib/
        BUILD
        src/
            lib.rs
```

`hello_lib/src/lib.rs`:
```rust
pub struct Greeter {
    greeting: String,
}

impl Greeter {
    pub fn new(greeting: &str) -> Greeter {
        Greeter { greeting: greeting.to_string(), }
    }

    pub fn greet(&self, thing: &str) -> String {
        format!("{} {}", &self.greeting, thing)
    }
}

#[cfg(test)]
mod test {
    use super::Greeter;

    #[test]
    fn test_greeting() {
        let hello = Greeter::new("Hi");
        assert_eq!("Hi Rust", hello.greet("Rust"));
    }
}
```

To build and run the tests, simply add a `rust_test` rule with no `srcs`
and only depends on the `hello_lib` `rust_library` target via the
`crate` attribute:

`hello_lib/BUILD`:
```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "hello_lib_test",
    crate = ":hello_lib",
    # You may add other deps that are specific to the test configuration
    deps = ["//some/dev/dep"],
)
```

Run the test with `bazel test //hello_lib:hello_lib_test`.

### Example: `test` directory

Integration tests that live in the [`tests` directory][int-tests], they are         essentially built as separate crates. Suppose you have the following directory         structure where `greeting.rs` is an integration test for the `hello_lib`         library crate:

[int-tests]: http://doc.rust-lang.org/book/testing.html#the-tests-directory

```output
[workspace]/
    WORKSPACE
    hello_lib/
        BUILD
        src/
            lib.rs
        tests/
            greeting.rs
```

`hello_lib/tests/greeting.rs`:
```rust
extern crate hello_lib;

use hello_lib;

#[test]
fn test_greeting() {
    let hello = greeter::Greeter::new("Hello");
    assert_eq!("Hello world", hello.greeting("world"));
}
```

To build the `greeting.rs` integration test, simply add a `rust_test` target
with `greeting.rs` in `srcs` and a dependency on the `hello_lib` target:

`hello_lib/BUILD`:
```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "greeting_test",
    srcs = ["tests/greeting.rs"],
    deps = [":hello_lib"],
)
```

Run the test with `bazel test //hello_lib:greeting_test`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_test-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_test-deps"></a>deps |  List of other libraries to be linked to this library target.<br><br>These can be either other `rust_library` targets or `cc_library` targets if linking a native library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-srcs"></a>srcs |  List of Rust `.rs` source files used to build the library.<br><br>If `srcs` contains more than one file, then there must be a file either named `lib.rs`. Otherwise, `crate_root` must be set to the source file that is the root of the crate to be passed to rustc to build this crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-data"></a>data |  List of files used by this rule at compile time and runtime.<br><br>If including data at compile time with include_str!() and similar, prefer `compile_data` over `data`, to prevent the data also being included in the runfiles.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-aliases"></a>aliases |  Remap crates to a new name or moniker for linkage to this target<br><br>These are other `rust_library` targets and will be presented as the new name given.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="rust_test-alwayslink"></a>alwayslink |  If 1, any binary that depends (directly or indirectly) on this library will link in all the object files even if some contain no symbols referenced by the binary.<br><br>This attribute is used by the C++ Starlark API when passing CcInfo providers.   | Boolean | optional |  `False`  |
| <a id="rust_test-compile_data"></a>compile_data |  List of files used by this rule at compile time.<br><br>This attribute can be used to specify any data files that are embedded into the library, such as via the [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html) macro.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-crate"></a>crate |  Target inline tests declared in the given crate<br><br>These tests are typically those that would be held out under `#[cfg(test)]` declarations.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_test-crate_features"></a>crate_features |  List of features to enable for this crate.<br><br>Features are defined in the code using the `#[cfg(feature = "foo")]` configuration option. The features listed here will be passed to `rustc` with `--cfg feature="${feature_name}"` flags.   | List of strings | optional |  `[]`  |
| <a id="rust_test-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_test-crate_root"></a>crate_root |  The file that will be passed to `rustc` to be used for building this crate.<br><br>If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary) or the single file in `srcs` if `srcs` contains only one file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_test-edition"></a>edition |  The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.   | String | optional |  `""`  |
| <a id="rust_test-env"></a>env |  Specifies additional environment variables to set when the test is executed by bazel test. Values are subject to `$(rootpath)`, `$(execpath)`, location, and ["Make variable"](https://docs.bazel.build/versions/master/be/make-variables.html) substitution.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_test-env_inherit"></a>env_inherit |  Specifies additional environment variables to inherit from the external environment when the test is executed by bazel test.   | List of strings | optional |  `[]`  |
| <a id="rust_test-experimental_use_cc_common_link"></a>experimental_use_cc_common_link |  Whether to use cc_common.link to link rust binaries. Possible values: [-1, 0, 1]. -1 means use the value of the toolchain.experimental_use_cc_common_link boolean build setting to determine. 0 means do not use cc_common.link (use rustc instead). 1 means use cc_common.link.   | Integer | optional |  `-1`  |
| <a id="rust_test-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Rust binaries linked with cc_common.link are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="rust_test-platform"></a>platform |  Optional platform to transition the test to.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_test-proc_macro_deps"></a>proc_macro_deps |  List of `rust_proc_macro` targets used to help build this library target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-rustc_env"></a>rustc_env |  Dictionary of additional `"key": "value"` environment variables to set for rustc.<br><br>rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the location of a generated file or external tool. Cargo build scripts that wish to expand locations should use cargo_build_script()'s build_script_env argument instead, as build scripts are run in a different environment - see cargo_build_script()'s documentation for more.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="rust_test-rustc_env_files"></a>rustc_env_files |  Files containing additional environment variables to set for rustc.<br><br>These files should  contain a single variable per line, of format `NAME=value`, and newlines may be included in a value by ending a line with a trailing back-slash (`\\`).<br><br>The order that these files will be processed is unspecified, so multiple definitions of a particular variable are discouraged.<br><br>Note that the variables here are subject to [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status) stamping should the `stamp` attribute be enabled. Stamp variables should be wrapped in brackets in order to be resolved. E.g. `NAME={WORKSPACE_STATUS_VARIABLE}`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_test-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |
| <a id="rust_test-stamp"></a>stamp |  Whether to encode build information into the `Rustc` action. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the `Rustc` action, even in             [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.             This setting should be avoided, since it potentially kills remote caching for the target and             any downstream actions that depend on it.<br><br>- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.<br><br>- `stamp = -1`: Embedding of build information is controlled by the             [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.<br><br>Stamped targets are not rebuilt unless their dependencies change.<br><br>For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped library won't be rebuilt when we change sources of the `rust_binary`. This is different from how [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp) behaves.   | Integer | optional |  `0`  |
| <a id="rust_test-use_libtest_harness"></a>use_libtest_harness |  Whether to use `libtest`. For targets using this flag, individual tests can be run by using the [--test_arg](https://docs.bazel.build/versions/4.0.0/command-line-reference.html#flag--test_arg) flag. E.g. `bazel test //src:rust_test --test_arg=foo::test::test_fn`.   | Boolean | optional |  `True`  |
| <a id="rust_test-version"></a>version |  A version to inject in the cargo environment variable.   | String | optional |  `"0.0.0"`  |


<a id="rust_test_suite"></a>

## rust_test_suite

<pre>
rust_test_suite(<a href="#rust_test_suite-name">name</a>, <a href="#rust_test_suite-srcs">srcs</a>, <a href="#rust_test_suite-shared_srcs">shared_srcs</a>, <a href="#rust_test_suite-kwargs">kwargs</a>)
</pre>

A rule for creating a test suite for a set of `rust_test` targets.

This rule can be used for setting up typical rust [integration tests][it]. Given the following
directory structure:

```text
[crate]/
    BUILD.bazel
    src/
        lib.rs
        main.rs
    tests/
        integrated_test_a.rs
        integrated_test_b.rs
        integrated_test_c.rs
        patterns/
            fibonacci_test.rs
        helpers/
            mod.rs
```

The rule can be used to generate [rust_test](#rust_test) targets for each source file under `tests`
and a [test_suite][ts] which encapsulates all tests.

```python
load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_test_suite")

rust_library(
    name = "math_lib",
    srcs = ["src/lib.rs"],
)

rust_binary(
    name = "math_bin",
    srcs = ["src/main.rs"],
)

rust_test_suite(
    name = "integrated_tests_suite",
    srcs = glob(["tests/**"]),
    shared_srcs=glob(["tests/helpers/**"]),
    deps = [":math_lib"],
)
```

[it]: https://doc.rust-lang.org/rust-by-example/testing/integration_testing.html
[ts]: https://docs.bazel.build/versions/master/be/general.html#test_suite


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_test_suite-name"></a>name |  The name of the `test_suite`.   |  none |
| <a id="rust_test_suite-srcs"></a>srcs |  All test sources, typically `glob(["tests/**/*.rs"])`.   |  none |
| <a id="rust_test_suite-shared_srcs"></a>shared_srcs |  Optional argument for sources shared among tests, typically helper functions.   |  `[]` |
| <a id="rust_test_suite-kwargs"></a>kwargs |  Additional keyword arguments for the underyling [rust_test](#rust_test) targets. The `tags` argument is also passed to the generated `test_suite` target.   |  none |


