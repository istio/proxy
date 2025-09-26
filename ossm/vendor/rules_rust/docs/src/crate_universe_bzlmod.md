<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# Crate Universe

Crate Universe is a set of Bazel rule for generating Rust targets using Cargo.

This doc describes using crate_universe with bzlmod.

If you're using a WORKSPACE file, please see [the WORKSPACE equivalent of this doc](crate_universe.html).

There are some examples of using crate_universe with bzlmod in the [example folder](../examples/bzlmod).

# Table of Contents

1. [Setup](#Setup)
2. [Dependencies](#dependencies)
    * [Cargo Workspace](#cargo-workspaces)
    * [Direct Packages](#direct-dependencies)
    * [Vendored Dependencies](#vendored-dependencies)
3. [Crate reference](#crate)
   * [from_cargo](#from_cargo)
   * [from_specs](#from_specs)


## Setup

To use rules_rust in a project using bzlmod, add the following to your MODULE.bazel file:

```python
bazel_dep(name = "rules_rust", version = "0.49.3")
```

You find the latest version on the [release page](https://github.com/bazelbuild/rules_rust/releases).


After adding `rules_rust` in your MODULE.bazel, set the following to begin using `crate_universe`:

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")
//  # ... Dependencies
use_repo(crate, "crates")
```

## Dependencies

There are three different ways to declare dependencies in your MODULE.

1) Cargo workspace
2) Direct Dependencies
3) Vendored Dependencies

### Cargo Workspaces

One of the simpler ways to wire up dependencies would be to first structure your project into a Cargo workspace.
The crates_repository rule can ingest a root Cargo.toml file and generate Bazel dependencies from there.
You find a complete example in the in the [example folder](../examples/bzlmod/all_crate_deps).

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")

crate.from_cargo(
    name = "crates",
    cargo_lockfile = "//:Cargo.lock",
    manifests = ["//:Cargo.toml"],
)
use_repo(crate, "crates")
```

The generated crates_repository contains helper macros which make collecting dependencies for Bazel targets simpler.
Notably, the all_crate_deps and aliases macros (
see [Dependencies API](https://bazelbuild.github.io/rules_rust/crate_universe.html#dependencies-api)) commonly allow the
Cargo.toml files to be the single source of truth for dependencies.
Since these macros come from the generated repository, the dependencies and alias definitions
they return will automatically update BUILD targets. In your BUILD files,
you use these macros for a Rust library as shown below:

```python
load("@crate_index//:defs.bzl", "aliases", "all_crate_deps")
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "lib",
    aliases = aliases(),
    deps = all_crate_deps(
        normal = True,
    ),
    proc_macro_deps = all_crate_deps(
        proc_macro = True,
    ),
)

rust_test(
    name = "unit_test",
    crate = ":lib",
    aliases = aliases(
        normal_dev = True,
        proc_macro_dev = True,
    ),
    deps = all_crate_deps(
        normal_dev = True,
    ),
    proc_macro_deps = all_crate_deps(
        proc_macro_dev = True,
    ),
)
```

For a Rust binary that does not depend on any macro, use the following configuration
in your build file:

```python
rust_binary(
    name = "bin",
    srcs = ["src/main.rs"],
    deps = all_crate_deps(normal = True),
)
```

You have to repin before your first build to ensure all Bazel targets for the macros
are generated.

Dependency syncing and updating is done in the repository rule which means it's done during the
analysis phase of builds. As mentioned in the environments variable table above, the `CARGO_BAZEL_REPIN`
(or `REPIN`) environment variables can be used to force the rule to update dependencies and potentially
render a new lockfile. Given an instance of this repository rule named `crates`, the easiest way to
repin dependencies is to run:

```shell
CARGO_BAZEL_REPIN=1 bazel sync --only=crates
```

This will result in all dependencies being updated for a project. The `CARGO_BAZEL_REPIN`
environment variable can also be used to customize how dependencies are updated.
For more details about repin, [please refer to the documentation](https://bazelbuild.github.io/rules_rust/crate_universe.html#crates_vendor).

### Direct Dependencies

In cases where Rust targets have heavy interactions with other Bazel targets ([Cc](https://docs.bazel.build/versions/main/be/c-cpp.html), [Proto](https://rules-proto-grpc.com/en/4.5.0/lang/rust.html),
etc.), maintaining Cargo.toml files may have diminishing returns as things like rust-analyzer
begin to be confused about missing targets or environment variables defined only in Bazel.
In situations like this, it may be desirable to have a âCargo freeâ setup. You find an example in the in the [example folder](../examples/bzlmod/hello_world_no_cargo).

crates_repository supports this through the packages attribute,
as shown below.

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")

crate.spec(package = "serde", features = ["derive"], version = "1.0")
crate.spec(package = "serde_json", version = "1.0")
crate.spec(package = "tokio", default_features=False, features = ["macros", "net", "rt-multi-thread"], version = "1.38")

crate.from_specs()
use_repo(crate, "crates")
```

Consuming dependencies may be more ergonomic in this case through the aliases defined in the new repository.
In your BUILD files, you use direct dependencies as shown below:

```python
rust_binary(
    name = "bin",
    crate_root = "src/main.rs",
    srcs = glob([
        "src/*.rs",
    ]),
    deps = [
        # External crates
        "@crates//:serde",
        "@crates//:serde_json",
        "@crates//:tokio",
    ],
    visibility = ["//visibility:public"],
)
```

Notice, direct dependencies do not need repining.
Only a cargo workspace needs updating whenever the underlying Cargo.toml file changed.

### Vendored Dependencies

In some cases, it is require that all external dependencies are vendored, meaning downloaded
and stored in the workspace. This helps, for example, to conduct licence scans, apply custom patches,
or to ensure full build reproducibility since no download error could possibly occur.
You find a complete example in the in the [example folder](../examples/bzlmod/all_deps_vendor).

For the setup, you need to add the skylib in addition to the rust rules to your MODUE.bazel.

```python
module(
    name = "deps_vendored",
    version = "0.0.0"
)
###############################################################################
# B A Z E L  C E N T R A L  R E G I S T R Y # https://registry.bazel.build/
###############################################################################
# https://github.com/bazelbuild/bazel-skylib/releases/
bazel_dep(name = "bazel_skylib", version = "1.7.1")

# https://github.com/bazelbuild/rules_rust/releases
bazel_dep(name = "rules_rust", version = "0.49.3")

###############################################################################
# T O O L C H A I N S
###############################################################################

# Rust toolchain
RUST_EDITION = "2021"
RUST_VERSION = "1.80.1"

rust = use_extension("@rules_rust//rust:extensions.bzl", "rust")
rust.toolchain(
    edition = RUST_EDITION,
    versions = [RUST_VERSION],
)
use_repo(rust, "rust_toolchains")
register_toolchains("@rust_toolchains//:all")

###############################################################################
# R U S T  C R A T E S
###############################################################################
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")
```

Note, it is important to load the crate_universe rules otherwise you will get an error
as the rule set is needed in the vendored target.

Assuming you have a package called `basic` in which you want to vendor dependencies,
then you create a folder `basic/3rdparty`. The folder name can be arbitrary,
but by convention, its either thirdparty or 3rdparty to indicate vendored dependencies.
In the 3rdparty folder, you add a target crates_vendor to declare your dependencies to vendor.
In the example, we vendor a specific version of bzip2.

```python
load("@rules_rust//crate_universe:defs.bzl", "crate", "crates_vendor")

crates_vendor(
    name = "crates_vendor",
    annotations = {
        "bzip2-sys": [crate.annotation(
            gen_build_script = True,
        )],
    },
    cargo_lockfile = "Cargo.Bazel.lock",
    generate_build_scripts = False,
    mode = "remote",
    packages = {
        "bzip2": crate.spec(
            version = "=0.3.3",
        ),
    },
    repository_name = "basic",
    tags = ["manual"],
)
```

Next, you have to run `Cargo build` to generate a Cargo.lock file with all resolved dependencies.
Then, you rename Cargo.lock to Cargo.Bazel.lock and place it inside the `basic/3rdparty` folder.

At this point, you have the following folder and files:

```
basic
    âââ 3rdparty
    â   âââ BUILD.bazel
    â   âââ Cargo.Bazel.lock
```

Now you can run the `crates_vendor` target:

`bazel run //basic/3rdparty:crates_vendor`

This generates a crate folders with all configurations for the vendored dependencies.

```
basic
    âââ 3rdparty
    â   âââ cratea
    â   âââ BUILD.bazel
    â   âââ Cargo.Bazel.lock
```

Suppose you have an application in `basic/src` that is defined in `basic/BUILD.bazel` and
that depends on a vendored dependency. You find a list of all available vendored dependencies
in the BUILD file of the generated folder: `basic/3rdparty/crates/BUILD.bazel`
You declare a vendored dependency in you target as following:

```python
load("@rules_rust//rust:defs.bzl", "rust_binary")

rust_binary(
    name = "hello_sys",
    srcs = ["src/main.rs"],
    deps = ["//basic/3rdparty/crates:bzip2"],
    visibility = ["//visibility:public"],
)
```
Note, the vendored dependency is not yet accessible because you have to define first
how to load the vendored dependencies. For that, you first create a file `sys_deps.bzl`
and add the following content:

```python
# rename the default name "crate_repositories" in case you import multiple vendored folders.
load("//basic/3rdparty/crates:defs.bzl", basic_crate_repositories = "crate_repositories")

def sys_deps():
    # Load the vendored dependencies
    basic_crate_repositories()
```

This is straightforward, you import the generated crate_repositories from the crates folder,
rename it to avoid name clashes in case you import from multiple vendored folders, and then
just load the vendored dependencies.

In a WORKSPACE configuration, you would just load and call sys_deps(), but in a MODULE configuration, you cannot do that.
Instead, you create a new file `WORKSPACE.bzlmod` and add the following content.

```python
load("//:sys_deps.bzl", "sys_deps")
sys_deps()
```

Now, you can build the project as usual.

There are some more examples of using crate_universe with bzlmod in the [example folder](https://github.com/bazelbuild/rules_rust/blob/main/examples/bzlmod/).

<a id="crate"></a>

## crate

<pre>
crate = use_extension("@rules_rust//crate_universe:docs_bzlmod.bzl", "crate")
crate.annotation(<a href="#crate.annotation-deps">deps</a>, <a href="#crate.annotation-data">data</a>, <a href="#crate.annotation-additive_build_file">additive_build_file</a>, <a href="#crate.annotation-additive_build_file_content">additive_build_file_content</a>, <a href="#crate.annotation-alias_rule">alias_rule</a>,
                 <a href="#crate.annotation-build_script_data">build_script_data</a>, <a href="#crate.annotation-build_script_data_glob">build_script_data_glob</a>, <a href="#crate.annotation-build_script_deps">build_script_deps</a>, <a href="#crate.annotation-build_script_env">build_script_env</a>,
                 <a href="#crate.annotation-build_script_proc_macro_deps">build_script_proc_macro_deps</a>, <a href="#crate.annotation-build_script_rundir">build_script_rundir</a>, <a href="#crate.annotation-build_script_rustc_env">build_script_rustc_env</a>,
                 <a href="#crate.annotation-build_script_toolchains">build_script_toolchains</a>, <a href="#crate.annotation-build_script_tools">build_script_tools</a>, <a href="#crate.annotation-compile_data">compile_data</a>, <a href="#crate.annotation-compile_data_glob">compile_data_glob</a>, <a href="#crate.annotation-crate">crate</a>,
                 <a href="#crate.annotation-crate_features">crate_features</a>, <a href="#crate.annotation-data_glob">data_glob</a>, <a href="#crate.annotation-disable_pipelining">disable_pipelining</a>, <a href="#crate.annotation-extra_aliased_targets">extra_aliased_targets</a>,
                 <a href="#crate.annotation-gen_all_binaries">gen_all_binaries</a>, <a href="#crate.annotation-gen_binaries">gen_binaries</a>, <a href="#crate.annotation-gen_build_script">gen_build_script</a>, <a href="#crate.annotation-override_target_bin">override_target_bin</a>,
                 <a href="#crate.annotation-override_target_build_script">override_target_build_script</a>, <a href="#crate.annotation-override_target_lib">override_target_lib</a>, <a href="#crate.annotation-override_target_proc_macro">override_target_proc_macro</a>,
                 <a href="#crate.annotation-patch_args">patch_args</a>, <a href="#crate.annotation-patch_tool">patch_tool</a>, <a href="#crate.annotation-patches">patches</a>, <a href="#crate.annotation-proc_macro_deps">proc_macro_deps</a>, <a href="#crate.annotation-repositories">repositories</a>, <a href="#crate.annotation-rustc_env">rustc_env</a>,
                 <a href="#crate.annotation-rustc_env_files">rustc_env_files</a>, <a href="#crate.annotation-rustc_flags">rustc_flags</a>, <a href="#crate.annotation-shallow_since">shallow_since</a>, <a href="#crate.annotation-version">version</a>)
crate.from_cargo(<a href="#crate.from_cargo-name">name</a>, <a href="#crate.from_cargo-cargo_config">cargo_config</a>, <a href="#crate.from_cargo-cargo_lockfile">cargo_lockfile</a>, <a href="#crate.from_cargo-generate_binaries">generate_binaries</a>, <a href="#crate.from_cargo-generate_build_scripts">generate_build_scripts</a>,
                 <a href="#crate.from_cargo-manifests">manifests</a>, <a href="#crate.from_cargo-splicing_config">splicing_config</a>, <a href="#crate.from_cargo-supported_platform_triples">supported_platform_triples</a>)
crate.from_specs(<a href="#crate.from_specs-name">name</a>, <a href="#crate.from_specs-cargo_config">cargo_config</a>, <a href="#crate.from_specs-generate_binaries">generate_binaries</a>, <a href="#crate.from_specs-generate_build_scripts">generate_build_scripts</a>, <a href="#crate.from_specs-splicing_config">splicing_config</a>,
                 <a href="#crate.from_specs-supported_platform_triples">supported_platform_triples</a>)
crate.spec(<a href="#crate.spec-artifact">artifact</a>, <a href="#crate.spec-branch">branch</a>, <a href="#crate.spec-default_features">default_features</a>, <a href="#crate.spec-features">features</a>, <a href="#crate.spec-git">git</a>, <a href="#crate.spec-lib">lib</a>, <a href="#crate.spec-package">package</a>, <a href="#crate.spec-rev">rev</a>, <a href="#crate.spec-tag">tag</a>, <a href="#crate.spec-version">version</a>)
</pre>

Crate universe module extensions.


**TAG CLASSES**

<a id="crate.annotation"></a>

### annotation

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crate.annotation-deps"></a>deps |  A list of labels to add to a crate's `rust_library::deps` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-data"></a>data |  A list of labels to add to a crate's `rust_library::data` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-additive_build_file"></a>additive_build_file |  A file containing extra contents to write to the bottom of generated BUILD files.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.annotation-additive_build_file_content"></a>additive_build_file_content |  Extra contents to write to the bottom of generated BUILD files.   | String | optional |  `""`  |
| <a id="crate.annotation-alias_rule"></a>alias_rule |  Alias rule to use instead of `native.alias()`.  Overrides [render_config](#render_config)'s 'default_alias_rule'.   | String | optional |  `""`  |
| <a id="crate.annotation-build_script_data"></a>build_script_data |  A list of labels to add to a crate's `cargo_build_script::data` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-build_script_data_glob"></a>build_script_data_glob |  A list of glob patterns to add to a crate's `cargo_build_script::data` attribute   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-build_script_deps"></a>build_script_deps |  A list of labels to add to a crate's `cargo_build_script::deps` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-build_script_env"></a>build_script_env |  Additional environment variables to set on a crate's `cargo_build_script::env` attribute.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crate.annotation-build_script_proc_macro_deps"></a>build_script_proc_macro_deps |  A list of labels to add to a crate's `cargo_build_script::proc_macro_deps` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-build_script_rundir"></a>build_script_rundir |  An override for the build script's rundir attribute.   | String | optional |  `""`  |
| <a id="crate.annotation-build_script_rustc_env"></a>build_script_rustc_env |  Additional environment variables to set on a crate's `cargo_build_script::env` attribute.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crate.annotation-build_script_toolchains"></a>build_script_toolchains |  A list of labels to set on a crates's `cargo_build_script::toolchains` attribute.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="crate.annotation-build_script_tools"></a>build_script_tools |  A list of labels to add to a crate's `cargo_build_script::tools` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-compile_data"></a>compile_data |  A list of labels to add to a crate's `rust_library::compile_data` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-compile_data_glob"></a>compile_data_glob |  A list of glob patterns to add to a crate's `rust_library::compile_data` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-crate"></a>crate |  The name of the crate the annotation is applied to   | String | required |  |
| <a id="crate.annotation-crate_features"></a>crate_features |  A list of strings to add to a crate's `rust_library::crate_features` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-data_glob"></a>data_glob |  A list of glob patterns to add to a crate's `rust_library::data` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-disable_pipelining"></a>disable_pipelining |  If True, disables pipelining for library targets for this crate.   | Boolean | optional |  `False`  |
| <a id="crate.annotation-extra_aliased_targets"></a>extra_aliased_targets |  A list of targets to add to the generated aliases in the root crate_universe repository.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crate.annotation-gen_all_binaries"></a>gen_all_binaries |  If true, generates `rust_binary` targets for all of the crates bins   | Boolean | optional |  `False`  |
| <a id="crate.annotation-gen_binaries"></a>gen_binaries |  As a list, the subset of the crate's bins that should get `rust_binary` targets produced.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-gen_build_script"></a>gen_build_script |  An authorative flag to determine whether or not to produce `cargo_build_script` targets for the current crate. Supported values are 'on', 'off', and 'auto'.   | String | optional |  `"auto"`  |
| <a id="crate.annotation-override_target_bin"></a>override_target_bin |  An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.annotation-override_target_build_script"></a>override_target_build_script |  An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.annotation-override_target_lib"></a>override_target_lib |  An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.annotation-override_target_proc_macro"></a>override_target_proc_macro |  An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.annotation-patch_args"></a>patch_args |  The `patch_args` attribute of a Bazel repository rule. See [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-patch_tool"></a>patch_tool |  The `patch_tool` attribute of a Bazel repository rule. See [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)   | String | optional |  `""`  |
| <a id="crate.annotation-patches"></a>patches |  The `patches` attribute of a Bazel repository rule. See [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="crate.annotation-proc_macro_deps"></a>proc_macro_deps |  A list of labels to add to a crate's `rust_library::proc_macro_deps` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-repositories"></a>repositories |  A list of repository names specified from `crate.from_cargo(name=...)` that this annotation is applied to. Defaults to all repositories.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-rustc_env"></a>rustc_env |  Additional variables to set on a crate's `rust_library::rustc_env` attribute.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crate.annotation-rustc_env_files"></a>rustc_env_files |  A list of labels to set on a crate's `rust_library::rustc_env_files` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-rustc_flags"></a>rustc_flags |  A list of strings to set on a crate's `rust_library::rustc_flags` attribute.   | List of strings | optional |  `[]`  |
| <a id="crate.annotation-shallow_since"></a>shallow_since |  An optional timestamp used for crates originating from a git repository instead of a crate registry. This flag optimizes fetching the source code.   | String | optional |  `""`  |
| <a id="crate.annotation-version"></a>version |  The versions of the crate the annotation is applied to. Defaults to all versions.   | String | optional |  `"*"`  |

<a id="crate.from_cargo"></a>

### from_cargo

Generates a repo @crates from a Cargo.toml / Cargo.lock pair.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crate.from_cargo-name"></a>name |  The name of the repo to generate   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"crates"`  |
| <a id="crate.from_cargo-cargo_config"></a>cargo_config |  A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.from_cargo-cargo_lockfile"></a>cargo_lockfile |  The path to an existing `Cargo.lock` file   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.from_cargo-generate_binaries"></a>generate_binaries |  Whether to generate `rust_binary` targets for all the binary crates in every package. By default only the `rust_library` targets are generated.   | Boolean | optional |  `False`  |
| <a id="crate.from_cargo-generate_build_scripts"></a>generate_build_scripts |  Whether or not to generate [cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html) by default.   | Boolean | optional |  `True`  |
| <a id="crate.from_cargo-manifests"></a>manifests |  A list of Cargo manifests (`Cargo.toml` files).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="crate.from_cargo-splicing_config"></a>splicing_config |  The configuration flags to use for splicing Cargo maniests. Use `//crate_universe:defs.bzl\%rsplicing_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crate.from_cargo-supported_platform_triples"></a>supported_platform_triples |  A set of all platform triples to consider when generating dependencies.   | List of strings | optional |  `["aarch64-apple-darwin", "aarch64-apple-ios", "aarch64-apple-ios-sim", "aarch64-linux-android", "aarch64-pc-windows-msvc", "aarch64-unknown-fuchsia", "aarch64-unknown-linux-gnu", "aarch64-unknown-nixos-gnu", "aarch64-unknown-nto-qnx710", "arm-unknown-linux-gnueabi", "armv7-linux-androideabi", "armv7-unknown-linux-gnueabi", "i686-apple-darwin", "i686-linux-android", "i686-pc-windows-msvc", "i686-unknown-freebsd", "i686-unknown-linux-gnu", "powerpc-unknown-linux-gnu", "riscv32imc-unknown-none-elf", "riscv64gc-unknown-none-elf", "s390x-unknown-linux-gnu", "thumbv7em-none-eabi", "thumbv8m.main-none-eabi", "wasm32-unknown-unknown", "wasm32-wasip1", "x86_64-apple-darwin", "x86_64-apple-ios", "x86_64-linux-android", "x86_64-pc-windows-msvc", "x86_64-unknown-freebsd", "x86_64-unknown-fuchsia", "x86_64-unknown-linux-gnu", "x86_64-unknown-nixos-gnu", "x86_64-unknown-none"]`  |

<a id="crate.from_specs"></a>

### from_specs

Generates a repo @crates from the defined `spec` tags.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crate.from_specs-name"></a>name |  The name of the repo to generate.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"crates"`  |
| <a id="crate.from_specs-cargo_config"></a>cargo_config |  A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crate.from_specs-generate_binaries"></a>generate_binaries |  Whether to generate `rust_binary` targets for all the binary crates in every package. By default only the `rust_library` targets are generated.   | Boolean | optional |  `False`  |
| <a id="crate.from_specs-generate_build_scripts"></a>generate_build_scripts |  Whether or not to generate [cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html) by default.   | Boolean | optional |  `True`  |
| <a id="crate.from_specs-splicing_config"></a>splicing_config |  The configuration flags to use for splicing Cargo maniests. Use `//crate_universe:defs.bzl\%rsplicing_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crate.from_specs-supported_platform_triples"></a>supported_platform_triples |  A set of all platform triples to consider when generating dependencies.   | List of strings | optional |  `["aarch64-apple-darwin", "aarch64-apple-ios", "aarch64-apple-ios-sim", "aarch64-linux-android", "aarch64-pc-windows-msvc", "aarch64-unknown-fuchsia", "aarch64-unknown-linux-gnu", "aarch64-unknown-nixos-gnu", "aarch64-unknown-nto-qnx710", "arm-unknown-linux-gnueabi", "armv7-linux-androideabi", "armv7-unknown-linux-gnueabi", "i686-apple-darwin", "i686-linux-android", "i686-pc-windows-msvc", "i686-unknown-freebsd", "i686-unknown-linux-gnu", "powerpc-unknown-linux-gnu", "riscv32imc-unknown-none-elf", "riscv64gc-unknown-none-elf", "s390x-unknown-linux-gnu", "thumbv7em-none-eabi", "thumbv8m.main-none-eabi", "wasm32-unknown-unknown", "wasm32-wasip1", "x86_64-apple-darwin", "x86_64-apple-ios", "x86_64-linux-android", "x86_64-pc-windows-msvc", "x86_64-unknown-freebsd", "x86_64-unknown-fuchsia", "x86_64-unknown-linux-gnu", "x86_64-unknown-nixos-gnu", "x86_64-unknown-none"]`  |

<a id="crate.spec"></a>

### spec

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crate.spec-artifact"></a>artifact |  Set to 'bin' to pull in a binary crate as an artifact dependency. Requires a nightly Cargo.   | String | optional |  `""`  |
| <a id="crate.spec-branch"></a>branch |  The git branch of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.   | String | optional |  `""`  |
| <a id="crate.spec-default_features"></a>default_features |  Maps to the `default-features` flag.   | Boolean | optional |  `True`  |
| <a id="crate.spec-features"></a>features |  A list of features to use for the crate.   | List of strings | optional |  `[]`  |
| <a id="crate.spec-git"></a>git |  The Git url to use for the crate. Cannot be used with `version`.   | String | optional |  `""`  |
| <a id="crate.spec-lib"></a>lib |  If using `artifact = 'bin'`, additionally setting `lib = True` declares a dependency on both the package's library and binary, as opposed to just the binary.   | Boolean | optional |  `False`  |
| <a id="crate.spec-package"></a>package |  The explicit name of the package.   | String | required |  |
| <a id="crate.spec-rev"></a>rev |  The git revision of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified.   | String | optional |  `""`  |
| <a id="crate.spec-tag"></a>tag |  The git tag of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.   | String | optional |  `""`  |
| <a id="crate.spec-version"></a>version |  The exact version of the crate. Cannot be used with `git`.   | String | optional |  `""`  |


