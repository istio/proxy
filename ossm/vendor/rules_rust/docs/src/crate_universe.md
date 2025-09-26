<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# Crate Universe

Crate Universe is a set of Bazel rule for generating Rust targets using Cargo.

This doc describes using crate_universe from a WORKSPACE file.

If you're using bzlmod, please see [the bzlmod equivalent of this doc](crate_universe_bzlmod.html).

## Setup

After loading `rules_rust` in your workspace, set the following to begin using `crate_universe`:

```python
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")

crate_universe_dependencies()
```

Note that if the current version of `rules_rust` is not a release artifact, you may need to set additional
flags such as [`bootstrap = True`](#crate_universe_dependencies-bootstrap) on the `crate_universe_dependencies`
call above or [crates_repository::generator_urls](#crates_repository-generator_urls) in uses of `crates_repository`.

## Rules

- [crates_repository](#crates_repository)
- [crates_vendor](#crates_vendor)

## Utility Macros

- [crate_universe_dependencies](#crate_universe_dependencies)
- [crate.annotation](#crateannotation)
- [crate.select](#crateselect)
- [crate.spec](#cratespec)
- [crate.workspace_member](#crateworkspace_member)
- [render_config](#render_config)
- [splicing_config](#splicing_config)

## Workflows

The [`crates_repository`](#crates_repository) rule (the primary repository rule of `rules_rust`'s cargo support) supports a number of different
ways users can express and organize their dependencies. The most common are listed below though there are more to be found in
the [./examples/crate_universe](https://github.com/bazelbuild/rules_rust/tree/main/examples/crate_universe) directory.

### Cargo Workspaces

One of the simpler ways to wire up dependencies would be to first structure your project into a [Cargo workspace][cw].
The `crates_repository` rule can ingest a root `Cargo.toml` file and generate dependencies from there.

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_repository")

crates_repository(
    name = "crate_index",
    cargo_lockfile = "//:Cargo.lock",
    lockfile = "//:Cargo.Bazel.lock",
    manifests = ["//:Cargo.toml"],
)

load("@crate_index//:defs.bzl", "crate_repositories")

crate_repositories()
```

The generated `crates_repository` contains helper macros which make collecting dependencies for Bazel targets simpler.
Notably, the `all_crate_deps` and `aliases` macros (see [Dependencies API](#dependencies-api)) commonly allow the
`Cargo.toml` files to be the single source of truth for dependencies. Since these macros come from the generated
repository, the dependencies and alias definitions they return will automatically update BUILD targets.

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

### Direct Packages

In cases where Rust targets have heavy interractions with other Bazel targests ([Cc][cc], [Proto][proto], etc.),
maintaining `Cargo.toml` files may have deminishing returns as things like [rust-analyzer][ra] begin to be confused
about missing targets or environment variables defined only in Bazel. In workspaces like this, it may be desirable
to have a "Cargo free" setup. `crates_repository` supports this through the `packages` attribute.

```python
load("@rules_rust//crate_universe:defs.bzl", "crate", "crates_repository", "render_config")

crates_repository(
    name = "crate_index",
    cargo_lockfile = "//:Cargo.lock",
    lockfile = "//:Cargo.Bazel.lock",
    packages = {
        "async-trait": crate.spec(
            version = "0.1.51",
        ),
        "mockall": crate.spec(
            version = "0.10.2",
        ),
        "tokio": crate.spec(
            version = "1.12.0",
        ),
    },
    # Setting the default package name to `""` forces the use of the macros defined in this repository
    # to always use the root package when looking for dependencies or aliases. This should be considered
    # optional as the repository also exposes alises for easy access to all dependencies.
    render_config = render_config(
        default_package_name = ""
    ),
)

load("@crate_index//:defs.bzl", "crate_repositories")

crate_repositories()
```

Consuming dependencies may be more ergonomic in this case through the aliases defined in the new repository.

```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "lib",
    deps = [
        "@crate_index//:tokio",
    ],
    proc_macro_deps = [
        "@crate_index//:async-trait",
    ],
)

rust_test(
    name = "unit_test",
    crate = ":lib",
    deps = [
        "@crate_index//:mockall",
    ],
)
```

### Binary dependencies

Neither of the above approaches supports depending on binary-only packages.

In order to depend on a Cargo package that contains binaries and no library, you
will need to do one of the following:

- Fork the package to add an empty lib.rs, which makes the package visible to
  Cargo metadata and compatible with the above approaches;

- Or handwrite your own build target for the binary, use `http_archive` to
  import its source code, and use `crates_repository` to make build targets for
  its dependencies. This is demonstrated below using the `rustfilt` crate as an
  example.

```python
# in WORKSPACE.bazel

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rustfilt",
    build_file = "//rustfilt:BUILD.rustfilt.bazel",
    sha256 = "c8d748b182c8f95224336d20dcc5609598af612581ce60cfb29da4dc8d0091f2",
    strip_prefix = "rustfilt-0.2.1",
    type = "tar.gz",
    urls = ["https://static.crates.io/crates/rustfilt/rustfilt-0.2.1.crate"],
)

load("@rules_rust//crate_universe:defs.bzl", "crates_repository")

crates_repository(
    name = "rustfilt_deps",
    cargo_lockfile = "//rustfilt:Cargo.lock",
    manifests = ["@rustfilt//:Cargo.toml"],
)

load("@rustfilt_deps//:defs.bzl", rustfilt_deps = "crate_repositories")

rustfilt_deps()
```

```python
# in rustfilt/BUILD.rustfilt.bazel

load("@rules_rust//rust:defs.bzl", "rust_binary")

rust_binary(
    name = "rustfilt",
    srcs = glob(["src/**/*.rs"]),
    edition = "2018",
    deps = [
        "@rustfilt_deps//:clap",
        "@rustfilt_deps//:lazy_static",
        "@rustfilt_deps//:regex",
        "@rustfilt_deps//:rustc-demangle",
    ],
)
```

If you use either `crates_repository` or `crates_vendor` to depend on a Cargo
package that contains _both_ a library crate _and_ binaries, by default only the
library gets made available to Bazel. To generate Bazel targets for the binary
crates as well, you must opt in to it with an annotation on the package:

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_repository", "crate")

crates_repository(
    name = "crate_index",
    annotations = {
        "thepackage": [crate.annotation(
            gen_binaries = True,
            # Or, to expose just a subset of the package's binaries by name:
            gen_binaries = ["rustfilt"],
        )],
    },
    # Or, to expose every binary of every package:
    generate_binaries = True,
    ...
)
```

## Dependencies API

After rendering dependencies, convenience macros may also be generated to provide
convenient accessors to larger sections of the dependency graph.

- [aliases](#aliases)
- [crate_deps](#crate_deps)
- [all_crate_deps](#all_crate_deps)
- [crate_repositories](#crate_repositories)

## Building crates with complicated dependencies

Some crates have build.rs scripts which are complicated to run. Typically these build C++ (or other languages), or attempt to find pre-installed libraries on the build machine.

There are a few approaches to making sure these run:

### Some things work without intervention

Some build scripts will happily run without any support needed.

rules_rust already supplies a configured C++ toolchain as input to build script execution, and sets variables like `CC`, `CXX`, `LD`, `LDFLAGS`, etc as needed. Many crates which invoke a compiler with the default environment, or forward these env vars, will Just Work (e.g. if using [`cc-rs`][cc-rs]).

rules_rust is open to PRs which make build scripts more likely to work by default with intervention assuming they're broadly applicable (e.g. setting extra widely-known env vars is probably fine, wiring up additional toolchains like `cmake` that aren't present by default for most Bazel users probably isn't).

### Supplying extra tools to build

Some build scripts can be made to work by pulling in some extra files and making them available to the build script.

Commonly this is done by passing the file to the `build_script_data` annotation for the crate, and using `build_script_env` to tell the build script where the file is. That env var may often use `$(execroot)` to get the path to the label, or `$${pwd}/` as a prefix if the path given is relative to the execroot (as will frequently happen when using a toolchain).A

There is an example of this in the "complicated dependencies" section of https://github.com/bazelbuild/rules_rust/blob/main/examples/crate_universe/WORKSPACE.bazel which builds libz-ng-sys.

### Building with Bazel and supplying via an override

Some build scripts have hooks to allow replacing parts that are complicated to build with output prepared by Bazel.

We can use those hooks by specifying paths (generally using the `build_script_data` and `build_script_env` annotations) and pointing them at labels which Bazel will then build. These env vars may often use `$(execroot)` to get the path to the label, or `$${pwd}/` as a prefix if the path given is relative to the execroot (as will frequently happen when using a toolchain).

There is an example of this in the "complicated dependencies" section of https://github.com/bazelbuild/rules_rust/blob/main/examples/crate_universe/WORKSPACE.bazel which builds boring-sys.

---

---

[cw]: https://doc.rust-lang.org/book/ch14-03-cargo-workspaces.html
[cc]: https://docs.bazel.build/versions/main/be/c-cpp.html
[proto]: https://rules-proto-grpc.com/en/latest/lang/rust.html
[ra]: https://rust-analyzer.github.io/
[cc-rs]: https://github.com/rust-lang/cc-rs

<a id="crates_vendor"></a>

## crates_vendor

<pre>
crates_vendor(<a href="#crates_vendor-name">name</a>, <a href="#crates_vendor-annotations">annotations</a>, <a href="#crates_vendor-bazel">bazel</a>, <a href="#crates_vendor-buildifier">buildifier</a>, <a href="#crates_vendor-cargo_bazel">cargo_bazel</a>, <a href="#crates_vendor-cargo_config">cargo_config</a>, <a href="#crates_vendor-cargo_lockfile">cargo_lockfile</a>,
              <a href="#crates_vendor-generate_binaries">generate_binaries</a>, <a href="#crates_vendor-generate_build_scripts">generate_build_scripts</a>, <a href="#crates_vendor-generate_target_compatible_with">generate_target_compatible_with</a>, <a href="#crates_vendor-manifests">manifests</a>,
              <a href="#crates_vendor-mode">mode</a>, <a href="#crates_vendor-packages">packages</a>, <a href="#crates_vendor-render_config">render_config</a>, <a href="#crates_vendor-repository_name">repository_name</a>, <a href="#crates_vendor-splicing_config">splicing_config</a>,
              <a href="#crates_vendor-supported_platform_triples">supported_platform_triples</a>, <a href="#crates_vendor-vendor_path">vendor_path</a>)
</pre>

A rule for defining Rust dependencies (crates) and writing targets for them to the current workspace.
This rule is useful for users whose workspaces are expected to be consumed in other workspaces as the
rendered `BUILD` files reduce the number of workspace dependencies, allowing for easier loads. This rule
handles all the same [workflows](#workflows) `crate_universe` rules do.

Example:

Given the following workspace structure:

```text
[workspace]/
    WORKSPACE
    BUILD
    Cargo.toml
    3rdparty/
        BUILD
    src/
        main.rs
```

The following is something that'd be found in `3rdparty/BUILD`:

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_vendor", "crate")

crates_vendor(
    name = "crates_vendor",
    annotations = {
        "rand": [crate.annotation(
            default_features = False,
            features = ["small_rng"],
        )],
    },
    cargo_lockfile = "//:Cargo.Bazel.lock",
    manifests = ["//:Cargo.toml"],
    mode = "remote",
    vendor_path = "crates",
    tags = ["manual"],
)
```

The above creates a target that can be run to write `BUILD` files into the `3rdparty`
directory next to where the target is defined. To run it, simply call:

```shell
bazel run //3rdparty:crates_vendor
```

<a id="#crates_vendor_repinning_updating_dependencies"></a>

### Repinning / Updating Dependencies

Repinning dependencies is controlled by both the `CARGO_BAZEL_REPIN` environment variable or the `--repin`
flag to the `crates_vendor` binary. To update dependencies, simply add the flag ro your `bazel run` invocation.

```shell
bazel run //3rdparty:crates_vendor -- --repin
```

Under the hood, `--repin` will trigger a [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html)
call against the generated workspace. The following table describes how to control particular values passed to the
`cargo update` command.

| Value | Cargo command |
| --- | --- |
| Any of [`true`, `1`, `yes`, `on`, `workspace`] | `cargo update --workspace` |
| Any of [`full`, `eager`, `all`] | `cargo update` |
| `package_name` | `cargo upgrade --package package_name` |
| `package_name@1.2.3` | `cargo upgrade --package package_name --precise 1.2.3` |

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crates_vendor-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="crates_vendor-annotations"></a>annotations |  Extra settings to apply to crates. See [crate.annotation](#crateannotation).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> List of strings</a> | optional |  `{}`  |
| <a id="crates_vendor-bazel"></a>bazel |  The path to a bazel binary used to locate the output_base for the current workspace.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crates_vendor-buildifier"></a>buildifier |  The path to a [buildifier](https://github.com/bazelbuild/buildtools/blob/5.0.1/buildifier/README.md) binary used to format generated BUILD files.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_rust//crate_universe/private/vendor:buildifier"`  |
| <a id="crates_vendor-cargo_bazel"></a>cargo_bazel |  The cargo-bazel binary to use for vendoring. If this attribute is not set, then a `CARGO_BAZEL_GENERATOR_PATH` action env will be used.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@@rules_rust~~cu~cargo_bazel_bootstrap//:binary"`  |
| <a id="crates_vendor-cargo_config"></a>cargo_config |  A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crates_vendor-cargo_lockfile"></a>cargo_lockfile |  The path to an existing `Cargo.lock` file   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crates_vendor-generate_binaries"></a>generate_binaries |  Whether to generate `rust_binary` targets for all the binary crates in every package. By default only the `rust_library` targets are generated.   | Boolean | optional |  `False`  |
| <a id="crates_vendor-generate_build_scripts"></a>generate_build_scripts |  Whether or not to generate [cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html) by default.   | Boolean | optional |  `True`  |
| <a id="crates_vendor-generate_target_compatible_with"></a>generate_target_compatible_with |  DEPRECATED: Moved to `render_config`.   | Boolean | optional |  `True`  |
| <a id="crates_vendor-manifests"></a>manifests |  A list of Cargo manifests (`Cargo.toml` files).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="crates_vendor-mode"></a>mode |  Flags determining how crates should be vendored. `local` is where crate source and BUILD files are written to the repository. `remote` is where only BUILD files are written and repository rules used to fetch source code.   | String | optional |  `"remote"`  |
| <a id="crates_vendor-packages"></a>packages |  A set of crates (packages) specifications to depend on. See [crate.spec](#crate.spec).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crates_vendor-render_config"></a>render_config |  The configuration flags to use for rendering. Use `//crate_universe:defs.bzl\%render_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crates_vendor-repository_name"></a>repository_name |  The name of the repository to generate for `remote` vendor modes. If unset, the label name will be used   | String | optional |  `""`  |
| <a id="crates_vendor-splicing_config"></a>splicing_config |  The configuration flags to use for splicing Cargo maniests. Use `//crate_universe:defs.bzl\%rsplicing_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crates_vendor-supported_platform_triples"></a>supported_platform_triples |  A set of all platform triples to consider when generating dependencies.   | List of strings | optional |  `["aarch64-apple-darwin", "aarch64-apple-ios", "aarch64-apple-ios-sim", "aarch64-linux-android", "aarch64-pc-windows-msvc", "aarch64-unknown-fuchsia", "aarch64-unknown-linux-gnu", "aarch64-unknown-nixos-gnu", "aarch64-unknown-nto-qnx710", "arm-unknown-linux-gnueabi", "armv7-linux-androideabi", "armv7-unknown-linux-gnueabi", "i686-apple-darwin", "i686-linux-android", "i686-pc-windows-msvc", "i686-unknown-freebsd", "i686-unknown-linux-gnu", "powerpc-unknown-linux-gnu", "riscv32imc-unknown-none-elf", "riscv64gc-unknown-none-elf", "s390x-unknown-linux-gnu", "thumbv7em-none-eabi", "thumbv8m.main-none-eabi", "wasm32-unknown-unknown", "wasm32-wasip1", "x86_64-apple-darwin", "x86_64-apple-ios", "x86_64-linux-android", "x86_64-pc-windows-msvc", "x86_64-unknown-freebsd", "x86_64-unknown-fuchsia", "x86_64-unknown-linux-gnu", "x86_64-unknown-nixos-gnu", "x86_64-unknown-none"]`  |
| <a id="crates_vendor-vendor_path"></a>vendor_path |  The path to a directory to write files into. Absolute paths will be treated as relative to the workspace root   | String | optional |  `"crates"`  |


<a id="aliases"></a>

## aliases

<pre>
aliases(<a href="#aliases-normal">normal</a>, <a href="#aliases-normal_dev">normal_dev</a>, <a href="#aliases-proc_macro">proc_macro</a>, <a href="#aliases-proc_macro_dev">proc_macro_dev</a>, <a href="#aliases-build">build</a>, <a href="#aliases-build_proc_macro">build_proc_macro</a>, <a href="#aliases-package_name">package_name</a>)
</pre>

Produces a map of Crate alias names to their original label

If no dependency kinds are specified, `normal` and `proc_macro` are used by default.
Setting any one flag will otherwise determine the contents of the returned dict.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="aliases-normal"></a>normal |  If True, normal dependencies are included in the output list.   |  `False` |
| <a id="aliases-normal_dev"></a>normal_dev |  If True, normal dev dependencies will be included in the output list..   |  `False` |
| <a id="aliases-proc_macro"></a>proc_macro |  If True, proc_macro dependencies are included in the output list.   |  `False` |
| <a id="aliases-proc_macro_dev"></a>proc_macro_dev |  If True, dev proc_macro dependencies are included in the output list.   |  `False` |
| <a id="aliases-build"></a>build |  If True, build dependencies are included in the output list.   |  `False` |
| <a id="aliases-build_proc_macro"></a>build_proc_macro |  If True, build proc_macro dependencies are included in the output list.   |  `False` |
| <a id="aliases-package_name"></a>package_name |  The package name of the set of dependencies to look up. Defaults to `native.package_name()` when unset.   |  `None` |

**RETURNS**

dict: The aliases of all associated packages


<a id="all_crate_deps"></a>

## all_crate_deps

<pre>
all_crate_deps(<a href="#all_crate_deps-normal">normal</a>, <a href="#all_crate_deps-normal_dev">normal_dev</a>, <a href="#all_crate_deps-proc_macro">proc_macro</a>, <a href="#all_crate_deps-proc_macro_dev">proc_macro_dev</a>, <a href="#all_crate_deps-build">build</a>, <a href="#all_crate_deps-build_proc_macro">build_proc_macro</a>,
               <a href="#all_crate_deps-package_name">package_name</a>)
</pre>

Finds the fully qualified label of all requested direct crate dependencies     for the package where this macro is called.

If no parameters are set, all normal dependencies are returned. Setting any one flag will
otherwise impact the contents of the returned list.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="all_crate_deps-normal"></a>normal |  If True, normal dependencies are included in the output list.   |  `False` |
| <a id="all_crate_deps-normal_dev"></a>normal_dev |  If True, normal dev dependencies will be included in the output list..   |  `False` |
| <a id="all_crate_deps-proc_macro"></a>proc_macro |  If True, proc_macro dependencies are included in the output list.   |  `False` |
| <a id="all_crate_deps-proc_macro_dev"></a>proc_macro_dev |  If True, dev proc_macro dependencies are included in the output list.   |  `False` |
| <a id="all_crate_deps-build"></a>build |  If True, build dependencies are included in the output list.   |  `False` |
| <a id="all_crate_deps-build_proc_macro"></a>build_proc_macro |  If True, build proc_macro dependencies are included in the output list.   |  `False` |
| <a id="all_crate_deps-package_name"></a>package_name |  The package name of the set of dependencies to look up. Defaults to `native.package_name()` when unset.   |  `None` |

**RETURNS**

list: A list of labels to generated rust targets (str)


<a id="crate.annotation"></a>

## crate.annotation

<pre>
crate.annotation(<a href="#crate.annotation-version">version</a>, <a href="#crate.annotation-additive_build_file">additive_build_file</a>, <a href="#crate.annotation-additive_build_file_content">additive_build_file_content</a>, <a href="#crate.annotation-alias_rule">alias_rule</a>,
                 <a href="#crate.annotation-build_script_compile_data">build_script_compile_data</a>, <a href="#crate.annotation-build_script_data">build_script_data</a>, <a href="#crate.annotation-build_script_tools">build_script_tools</a>,
                 <a href="#crate.annotation-build_script_data_glob">build_script_data_glob</a>, <a href="#crate.annotation-build_script_deps">build_script_deps</a>, <a href="#crate.annotation-build_script_env">build_script_env</a>,
                 <a href="#crate.annotation-build_script_proc_macro_deps">build_script_proc_macro_deps</a>, <a href="#crate.annotation-build_script_rundir">build_script_rundir</a>, <a href="#crate.annotation-build_script_rustc_env">build_script_rustc_env</a>,
                 <a href="#crate.annotation-build_script_toolchains">build_script_toolchains</a>, <a href="#crate.annotation-build_script_use_default_shell_env">build_script_use_default_shell_env</a>, <a href="#crate.annotation-compile_data">compile_data</a>,
                 <a href="#crate.annotation-compile_data_glob">compile_data_glob</a>, <a href="#crate.annotation-crate_features">crate_features</a>, <a href="#crate.annotation-data">data</a>, <a href="#crate.annotation-data_glob">data_glob</a>, <a href="#crate.annotation-deps">deps</a>, <a href="#crate.annotation-extra_aliased_targets">extra_aliased_targets</a>,
                 <a href="#crate.annotation-gen_binaries">gen_binaries</a>, <a href="#crate.annotation-disable_pipelining">disable_pipelining</a>, <a href="#crate.annotation-gen_build_script">gen_build_script</a>, <a href="#crate.annotation-patch_args">patch_args</a>, <a href="#crate.annotation-patch_tool">patch_tool</a>, <a href="#crate.annotation-patches">patches</a>,
                 <a href="#crate.annotation-proc_macro_deps">proc_macro_deps</a>, <a href="#crate.annotation-rustc_env">rustc_env</a>, <a href="#crate.annotation-rustc_env_files">rustc_env_files</a>, <a href="#crate.annotation-rustc_flags">rustc_flags</a>, <a href="#crate.annotation-shallow_since">shallow_since</a>,
                 <a href="#crate.annotation-override_targets">override_targets</a>)
</pre>

A collection of extra attributes and settings for a particular crate

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate.annotation-version"></a>version |  The version or semver-conditions to match with a crate. The wildcard `*` matches any version, including prerelease versions.   |  `"*"` |
| <a id="crate.annotation-additive_build_file"></a>additive_build_file |  A file containing extra contents to write to the bottom of generated BUILD files.   |  `None` |
| <a id="crate.annotation-additive_build_file_content"></a>additive_build_file_content |  Extra contents to write to the bottom of generated BUILD files.   |  `None` |
| <a id="crate.annotation-alias_rule"></a>alias_rule |  Alias rule to use instead of `native.alias()`.  Overrides [render_config](#render_config)'s 'default_alias_rule'.   |  `None` |
| <a id="crate.annotation-build_script_compile_data"></a>build_script_compile_data |  A list of labels to add to a crate's `cargo_build_script::compile_data` attribute.   |  `None` |
| <a id="crate.annotation-build_script_data"></a>build_script_data |  A list of labels to add to a crate's `cargo_build_script::data` attribute.   |  `None` |
| <a id="crate.annotation-build_script_tools"></a>build_script_tools |  A list of labels to add to a crate's `cargo_build_script::tools` attribute.   |  `None` |
| <a id="crate.annotation-build_script_data_glob"></a>build_script_data_glob |  A list of glob patterns to add to a crate's `cargo_build_script::data` attribute.   |  `None` |
| <a id="crate.annotation-build_script_deps"></a>build_script_deps |  A list of labels to add to a crate's `cargo_build_script::deps` attribute.   |  `None` |
| <a id="crate.annotation-build_script_env"></a>build_script_env |  Additional environment variables to set on a crate's `cargo_build_script::env` attribute.   |  `None` |
| <a id="crate.annotation-build_script_proc_macro_deps"></a>build_script_proc_macro_deps |  A list of labels to add to a crate's `cargo_build_script::proc_macro_deps` attribute.   |  `None` |
| <a id="crate.annotation-build_script_rundir"></a>build_script_rundir |  An override for the build script's rundir attribute.   |  `None` |
| <a id="crate.annotation-build_script_rustc_env"></a>build_script_rustc_env |  Additional environment variables to set on a crate's `cargo_build_script::env` attribute.   |  `None` |
| <a id="crate.annotation-build_script_toolchains"></a>build_script_toolchains |  A list of labels to set on a crates's `cargo_build_script::toolchains` attribute.   |  `None` |
| <a id="crate.annotation-build_script_use_default_shell_env"></a>build_script_use_default_shell_env |  Whether or not to include the default shell environment for the build script action.   |  `None` |
| <a id="crate.annotation-compile_data"></a>compile_data |  A list of labels to add to a crate's `rust_library::compile_data` attribute.   |  `None` |
| <a id="crate.annotation-compile_data_glob"></a>compile_data_glob |  A list of glob patterns to add to a crate's `rust_library::compile_data` attribute.   |  `None` |
| <a id="crate.annotation-crate_features"></a>crate_features |  A list of strings to add to a crate's `rust_library::crate_features` attribute.   |  `None` |
| <a id="crate.annotation-data"></a>data |  A list of labels to add to a crate's `rust_library::data` attribute.   |  `None` |
| <a id="crate.annotation-data_glob"></a>data_glob |  A list of glob patterns to add to a crate's `rust_library::data` attribute.   |  `None` |
| <a id="crate.annotation-deps"></a>deps |  A list of labels to add to a crate's `rust_library::deps` attribute.   |  `None` |
| <a id="crate.annotation-extra_aliased_targets"></a>extra_aliased_targets |  A list of targets to add to the generated aliases in the root crate_universe repository.   |  `None` |
| <a id="crate.annotation-gen_binaries"></a>gen_binaries |  As a list, the subset of the crate's bins that should get `rust_binary` targets produced. Or `True` to generate all, `False` to generate none.   |  `None` |
| <a id="crate.annotation-disable_pipelining"></a>disable_pipelining |  If True, disables pipelining for library targets for this crate.   |  `False` |
| <a id="crate.annotation-gen_build_script"></a>gen_build_script |  An authorative flag to determine whether or not to produce `cargo_build_script` targets for the current crate.   |  `None` |
| <a id="crate.annotation-patch_args"></a>patch_args |  The `patch_args` attribute of a Bazel repository rule. See [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)   |  `None` |
| <a id="crate.annotation-patch_tool"></a>patch_tool |  The `patch_tool` attribute of a Bazel repository rule. See [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)   |  `None` |
| <a id="crate.annotation-patches"></a>patches |  The `patches` attribute of a Bazel repository rule. See [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)   |  `None` |
| <a id="crate.annotation-proc_macro_deps"></a>proc_macro_deps |  A list of labels to add to a crate's `rust_library::proc_macro_deps` attribute.   |  `None` |
| <a id="crate.annotation-rustc_env"></a>rustc_env |  Additional variables to set on a crate's `rust_library::rustc_env` attribute.   |  `None` |
| <a id="crate.annotation-rustc_env_files"></a>rustc_env_files |  A list of labels to set on a crate's `rust_library::rustc_env_files` attribute.   |  `None` |
| <a id="crate.annotation-rustc_flags"></a>rustc_flags |  A list of strings to set on a crate's `rust_library::rustc_flags` attribute.   |  `None` |
| <a id="crate.annotation-shallow_since"></a>shallow_since |  An optional timestamp used for crates originating from a git repository instead of a crate registry. This flag optimizes fetching the source code.   |  `None` |
| <a id="crate.annotation-override_targets"></a>override_targets |  A dictionary of alternate targets to use when something depends on this crate to allow the parent repo to provide its own version of this dependency. Keys can be `proc-marco`, `custom-build`, `lib`, `bin`.   |  `None` |

**RETURNS**

string: A json encoded string containing the specified version and separately all other inputs.


<a id="crate.select"></a>

## crate.select

<pre>
crate.select(<a href="#crate.select-common">common</a>, <a href="#crate.select-selects">selects</a>)
</pre>

A Starlark Select for `crate.annotation()`.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate.select-common"></a>common |  A value that applies to all configurations.   |  none |
| <a id="crate.select-selects"></a>selects |  A dict of `target_triple` to values.   |  none |

**RETURNS**

struct: A struct representing the Starlark Select.


<a id="crate.spec"></a>

## crate.spec

<pre>
crate.spec(<a href="#crate.spec-package">package</a>, <a href="#crate.spec-version">version</a>, <a href="#crate.spec-artifact">artifact</a>, <a href="#crate.spec-lib">lib</a>, <a href="#crate.spec-default_features">default_features</a>, <a href="#crate.spec-features">features</a>, <a href="#crate.spec-git">git</a>, <a href="#crate.spec-branch">branch</a>, <a href="#crate.spec-tag">tag</a>, <a href="#crate.spec-rev">rev</a>)
</pre>

A constructor for a crate dependency.

See [specifying dependencies][sd] in the Cargo book for more details.

[sd]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate.spec-package"></a>package |  The explicit name of the package (used when attempting to alias a crate).   |  `None` |
| <a id="crate.spec-version"></a>version |  The exact version of the crate. Cannot be used with `git`.   |  `None` |
| <a id="crate.spec-artifact"></a>artifact |  Set to "bin" to pull in a binary crate as an artifact dependency. Requires a nightly Cargo.   |  `None` |
| <a id="crate.spec-lib"></a>lib |  If using `artifact = "bin"`, additionally setting `lib = True` declares a dependency on both the package's library and binary, as opposed to just the binary.   |  `None` |
| <a id="crate.spec-default_features"></a>default_features |  Maps to the `default-features` flag.   |  `True` |
| <a id="crate.spec-features"></a>features |  A list of features to use for the crate   |  `[]` |
| <a id="crate.spec-git"></a>git |  The Git url to use for the crate. Cannot be used with `version`.   |  `None` |
| <a id="crate.spec-branch"></a>branch |  The git branch of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.   |  `None` |
| <a id="crate.spec-tag"></a>tag |  The git tag of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.   |  `None` |
| <a id="crate.spec-rev"></a>rev |  The git revision of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified.   |  `None` |

**RETURNS**

string: A json encoded string of all inputs


<a id="crate.workspace_member"></a>

## crate.workspace_member

<pre>
crate.workspace_member(<a href="#crate.workspace_member-version">version</a>, <a href="#crate.workspace_member-sha256">sha256</a>)
</pre>

Define information for extra workspace members

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate.workspace_member-version"></a>version |  The semver of the crate to download. Must be an exact version.   |  none |
| <a id="crate.workspace_member-sha256"></a>sha256 |  The sha256 checksum of the `.crate` file.   |  `None` |

**RETURNS**

string: A json encoded string of all inputs


<a id="crate_deps"></a>

## crate_deps

<pre>
crate_deps(<a href="#crate_deps-deps">deps</a>, <a href="#crate_deps-package_name">package_name</a>)
</pre>

Finds the fully qualified label of the requested crates for the package where this macro is called.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate_deps-deps"></a>deps |  The desired list of crate targets.   |  none |
| <a id="crate_deps-package_name"></a>package_name |  The package name of the set of dependencies to look up. Defaults to `native.package_name()`.   |  `None` |

**RETURNS**

list: A list of labels to generated rust targets (str)


<a id="crate_repositories"></a>

## crate_repositories

<pre>
crate_repositories()
</pre>

A macro for defining repositories for all generated crates.


**RETURNS**

A list of repos visible to the module through the module extension.


<a id="crate_universe_dependencies"></a>

## crate_universe_dependencies

<pre>
crate_universe_dependencies(<a href="#crate_universe_dependencies-rust_version">rust_version</a>, <a href="#crate_universe_dependencies-bootstrap">bootstrap</a>, <a href="#crate_universe_dependencies-kwargs">kwargs</a>)
</pre>

Define dependencies of the `cargo-bazel` Rust target

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="crate_universe_dependencies-rust_version"></a>rust_version |  The version of rust to use when generating dependencies.   |  `"1.83.0"` |
| <a id="crate_universe_dependencies-bootstrap"></a>bootstrap |  If true, a `cargo_bootstrap_repository` target will be generated.   |  `False` |
| <a id="crate_universe_dependencies-kwargs"></a>kwargs |  Arguments to pass through to cargo_bazel_bootstrap.   |  none |

**RETURNS**

list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
  defined by this macro.


<a id="render_config"></a>

## render_config

<pre>
render_config(<a href="#render_config-build_file_template">build_file_template</a>, <a href="#render_config-crate_label_template">crate_label_template</a>, <a href="#render_config-crate_repository_template">crate_repository_template</a>,
              <a href="#render_config-crates_module_template">crates_module_template</a>, <a href="#render_config-default_alias_rule">default_alias_rule</a>, <a href="#render_config-default_package_name">default_package_name</a>,
              <a href="#render_config-generate_target_compatible_with">generate_target_compatible_with</a>, <a href="#render_config-platforms_template">platforms_template</a>, <a href="#render_config-regen_command">regen_command</a>, <a href="#render_config-vendor_mode">vendor_mode</a>,
              <a href="#render_config-generate_rules_license_metadata">generate_rules_license_metadata</a>)
</pre>

Various settings used to configure rendered outputs

The template parameters each support a select number of format keys. A description of each key
can be found below where the supported keys for each template can be found in the parameter docs

| key | definition |
| --- | --- |
| `name` | The name of the crate. Eg `tokio` |
| `repository` | The rendered repository name for the crate. Directly relates to `crate_repository_template`. |
| `triple` | A platform triple. Eg `x86_64-unknown-linux-gnu` |
| `version` | The crate version. Eg `1.2.3` |
| `target` | The library or binary target of the crate |
| `file` | The basename of a file |


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="render_config-build_file_template"></a>build_file_template |  The base template to use for BUILD file names. The available format keys are [`{name}`, {version}`].   |  `"//:BUILD.{name}-{version}.bazel"` |
| <a id="render_config-crate_label_template"></a>crate_label_template |  The base template to use for crate labels. The available format keys are [`{repository}`, `{name}`, `{version}`, `{target}`].   |  `"@{repository}__{name}-{version}//:{target}"` |
| <a id="render_config-crate_repository_template"></a>crate_repository_template |  The base template to use for Crate label repository names. The available format keys are [`{repository}`, `{name}`, `{version}`].   |  `"{repository}__{name}-{version}"` |
| <a id="render_config-crates_module_template"></a>crates_module_template |  The pattern to use for the `defs.bzl` and `BUILD.bazel` file names used for the crates module. The available format keys are [`{file}`].   |  `"//:{file}"` |
| <a id="render_config-default_alias_rule"></a>default_alias_rule |  Alias rule to use when generating aliases for all crates.  Acceptable values are 'alias', 'dbg'/'fastbuild'/'opt' (transitions each crate's `compilation_mode`)  or a string representing a rule in the form '<label to .bzl>:<rule>' that takes a single label parameter 'actual'. See '@crate_index//:alias_rules.bzl' for an example.   |  `"alias"` |
| <a id="render_config-default_package_name"></a>default_package_name |  The default package name to use in the rendered macros. This affects the auto package detection of things like `all_crate_deps`.   |  `None` |
| <a id="render_config-generate_target_compatible_with"></a>generate_target_compatible_with |  Whether to generate `target_compatible_with` annotations on the generated BUILD files.  This catches a `target_triple`being targeted that isn't declared in `supported_platform_triples`.   |  `True` |
| <a id="render_config-platforms_template"></a>platforms_template |  The base template to use for platform names. See [platforms documentation](https://docs.bazel.build/versions/main/platforms.html). The available format keys are [`{triple}`].   |  `"@rules_rust//rust/platform:{triple}"` |
| <a id="render_config-regen_command"></a>regen_command |  An optional command to demonstrate how generated files should be regenerated.   |  `None` |
| <a id="render_config-vendor_mode"></a>vendor_mode |  An optional configuration for rendirng content to be rendered into repositories.   |  `None` |
| <a id="render_config-generate_rules_license_metadata"></a>generate_rules_license_metadata |  Whether to generate rules license metedata   |  `False` |

**RETURNS**

string: A json encoded struct to match the Rust `config::RenderConfig` struct


<a id="splicing_config"></a>

## splicing_config

<pre>
splicing_config(<a href="#splicing_config-resolver_version">resolver_version</a>)
</pre>

Various settings used to configure Cargo manifest splicing behavior.

[rv]: https://doc.rust-lang.org/cargo/reference/resolver.html#resolver-versions


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="splicing_config-resolver_version"></a>resolver_version |  The [resolver version][rv] to use in generated Cargo manifests. This flag is **only** used when splicing a manifest from direct package definitions. See `crates_repository::packages`.   |  `"2"` |

**RETURNS**

str: A json encoded string of the parameters provided


<a id="crates_repository"></a>

## crates_repository

<pre>
crates_repository(<a href="#crates_repository-name">name</a>, <a href="#crates_repository-annotations">annotations</a>, <a href="#crates_repository-cargo_config">cargo_config</a>, <a href="#crates_repository-cargo_lockfile">cargo_lockfile</a>,
                  <a href="#crates_repository-compressed_windows_toolchain_names">compressed_windows_toolchain_names</a>, <a href="#crates_repository-generate_binaries">generate_binaries</a>, <a href="#crates_repository-generate_build_scripts">generate_build_scripts</a>,
                  <a href="#crates_repository-generate_target_compatible_with">generate_target_compatible_with</a>, <a href="#crates_repository-generator">generator</a>, <a href="#crates_repository-generator_sha256s">generator_sha256s</a>, <a href="#crates_repository-generator_urls">generator_urls</a>,
                  <a href="#crates_repository-isolated">isolated</a>, <a href="#crates_repository-lockfile">lockfile</a>, <a href="#crates_repository-manifests">manifests</a>, <a href="#crates_repository-packages">packages</a>, <a href="#crates_repository-quiet">quiet</a>, <a href="#crates_repository-render_config">render_config</a>, <a href="#crates_repository-repin_instructions">repin_instructions</a>,
                  <a href="#crates_repository-repo_mapping">repo_mapping</a>, <a href="#crates_repository-rust_toolchain_cargo_template">rust_toolchain_cargo_template</a>, <a href="#crates_repository-rust_toolchain_rustc_template">rust_toolchain_rustc_template</a>,
                  <a href="#crates_repository-rust_version">rust_version</a>, <a href="#crates_repository-splicing_config">splicing_config</a>, <a href="#crates_repository-supported_platform_triples">supported_platform_triples</a>)
</pre>

A rule for defining and downloading Rust dependencies (crates). This rule
handles all the same [workflows](#workflows) `crate_universe` rules do.

Environment Variables:

| variable | usage |
| --- | --- |
| `CARGO_BAZEL_GENERATOR_SHA256` | The sha256 checksum of the file located at `CARGO_BAZEL_GENERATOR_URL` |
| `CARGO_BAZEL_GENERATOR_URL` | The URL of a cargo-bazel binary. This variable takes precedence over attributes and can use `file://` for local paths |
| `CARGO_BAZEL_ISOLATED` | An authorative flag as to whether or not the `CARGO_HOME` environment variable should be isolated from the host configuration |
| `CARGO_BAZEL_REPIN` | An indicator that the dependencies represented by the rule should be regenerated. `REPIN` may also be used. See [Repinning / Updating Dependencies](#repinning--updating-dependencies) for more details. |
| `CARGO_BAZEL_REPIN_ONLY` | A comma-delimited allowlist for rules to execute repinning. Can be useful if multiple instances of the repository rule are used in a Bazel workspace, but repinning should be limited to one of them. |

Example:

Given the following workspace structure:

```text
[workspace]/
    WORKSPACE.bazel
    BUILD.bazel
    Cargo.toml
    Cargo.Bazel.lock
    src/
        main.rs
```

The following is something that'd be found in the `WORKSPACE` file:

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_repository", "crate")

crates_repository(
    name = "crate_index",
    annotations = {
        "rand": [crate.annotation(
            default_features = False,
            features = ["small_rng"],
        )],
    },
    cargo_lockfile = "//:Cargo.Bazel.lock",
    lockfile = "//:cargo-bazel-lock.json",
    manifests = ["//:Cargo.toml"],
    # Should match the version represented by the currently registered `rust_toolchain`.
    rust_version = "1.60.0",
)
```

The above will create an external repository which contains aliases and macros for accessing
Rust targets found in the dependency graph defined by the given manifests.

**NOTE**: The `cargo_lockfile` and `lockfile` must be manually created. The rule unfortunately does not yet create
it on its own. When initially setting up this rule, an empty file should be created and then
populated by repinning dependencies.

### Repinning / Updating Dependencies

Dependency syncing and updating is done in the repository rule which means it's done during the
analysis phase of builds. As mentioned in the environments variable table above, the `CARGO_BAZEL_REPIN`
(or `REPIN`) environment variables can be used to force the rule to update dependencies and potentially
render a new lockfile. Given an instance of this repository rule named `crate_index`, the easiest way to
repin dependencies is to run:

```shell
CARGO_BAZEL_REPIN=1 bazel sync --only=crate_index
```

This will result in all dependencies being updated for a project. The `CARGO_BAZEL_REPIN` environment variable
can also be used to customize how dependencies are updated. The following table shows translations from environment
variable values to the equivilant [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html) command
that is called behind the scenes to update dependencies.

| Value | Cargo command |
| --- | --- |
| Any of [`true`, `1`, `yes`, `on`, `workspace`] | `cargo update --workspace` |
| Any of [`full`, `eager`, `all`] | `cargo update` |
| `package_name` | `cargo upgrade --package package_name` |
| `package_name@1.2.3` | `cargo upgrade --package package_name@1.2.3` |
| `package_name@1.2.3=4.5.6` | `cargo upgrade --package package_name@1.2.3 --precise=4.5.6` |

If the `crates_repository` is used multiple times in the same Bazel workspace (e.g. for multiple independent
Rust workspaces), it may additionally be useful to use the `CARGO_BAZEL_REPIN_ONLY` environment variable, which
limits execution of the repinning to one or multiple instances of the `crates_repository` rule via a comma-delimited
allowlist:

```shell
CARGO_BAZEL_REPIN=1 CARGO_BAZEL_REPIN_ONLY=crate_index bazel sync --only=crate_index
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="crates_repository-name"></a>name |  A unique name for this repository.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="crates_repository-annotations"></a>annotations |  Extra settings to apply to crates. See [crate.annotation](#crateannotation).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> List of strings</a> | optional |  `{}`  |
| <a id="crates_repository-cargo_config"></a>cargo_config |  A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crates_repository-cargo_lockfile"></a>cargo_lockfile |  The path used to store the `crates_repository` specific [Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html) file. In the case that your `crates_repository` corresponds directly with an existing `Cargo.toml` file which has a paired `Cargo.lock` file, that `Cargo.lock` file should be used here, which will keep the versions used by cargo and bazel in sync.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="crates_repository-compressed_windows_toolchain_names"></a>compressed_windows_toolchain_names |  Wether or not the toolchain names of windows toolchains are expected to be in a `compressed` format.   | Boolean | optional |  `True`  |
| <a id="crates_repository-generate_binaries"></a>generate_binaries |  Whether to generate `rust_binary` targets for all the binary crates in every package. By default only the `rust_library` targets are generated.   | Boolean | optional |  `False`  |
| <a id="crates_repository-generate_build_scripts"></a>generate_build_scripts |  Whether or not to generate [cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html) by default.   | Boolean | optional |  `True`  |
| <a id="crates_repository-generate_target_compatible_with"></a>generate_target_compatible_with |  DEPRECATED: Moved to `render_config`.   | Boolean | optional |  `True`  |
| <a id="crates_repository-generator"></a>generator |  The absolute label of a generator. Eg. `@cargo_bazel_bootstrap//:cargo-bazel`. This is typically used when bootstrapping   | String | optional |  `""`  |
| <a id="crates_repository-generator_sha256s"></a>generator_sha256s |  Dictionary of `host_triple` -> `sha256` for a `cargo-bazel` binary.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crates_repository-generator_urls"></a>generator_urls |  URL template from which to download the `cargo-bazel` binary. `{host_triple}` and will be filled in according to the host platform.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crates_repository-isolated"></a>isolated |  If true, `CARGO_HOME` will be overwritten to a directory within the generated repository in order to prevent other uses of Cargo from impacting having any effect on the generated targets produced by this rule. For users who either have multiple `crate_repository` definitions in a WORKSPACE or rapidly re-pin dependencies, setting this to false may improve build times. This variable is also controled by `CARGO_BAZEL_ISOLATED` environment variable.   | Boolean | optional |  `True`  |
| <a id="crates_repository-lockfile"></a>lockfile |  The path to a file to use for reproducible renderings. If set, this file must exist within the workspace (but can be empty) before this rule will work.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="crates_repository-manifests"></a>manifests |  A list of Cargo manifests (`Cargo.toml` files).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="crates_repository-packages"></a>packages |  A set of crates (packages) specifications to depend on. See [crate.spec](#crate.spec).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="crates_repository-quiet"></a>quiet |  If stdout and stderr should not be printed to the terminal.   | Boolean | optional |  `True`  |
| <a id="crates_repository-render_config"></a>render_config |  The configuration flags to use for rendering. Use `//crate_universe:defs.bzl\%render_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crates_repository-repin_instructions"></a>repin_instructions |  Instructions to re-pin the repository if required. Many people have wrapper scripts for keeping dependencies up to date, and would like to point users to that instead of the default.   | String | optional |  `""`  |
| <a id="crates_repository-repo_mapping"></a>repo_mapping |  In `WORKSPACE` context only: a dictionary from local repository name to global repository name. This allows controls over workspace dependency resolution for dependencies of this repository.<br><br>For example, an entry `"@foo": "@bar"` declares that, for any time this repository depends on `@foo` (such as a dependency on `@foo//some:target`, it should actually resolve that dependency within globally-declared `@bar` (`@bar//some:target`).<br><br>This attribute is _not_ supported in `MODULE.bazel` context (when invoking a repository rule inside a module extension's implementation function).   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  |
| <a id="crates_repository-rust_toolchain_cargo_template"></a>rust_toolchain_cargo_template |  The template to use for finding the host `cargo` binary. `{version}` (eg. '1.53.0'), `{triple}` (eg. 'x86_64-unknown-linux-gnu'), `{arch}` (eg. 'aarch64'), `{vendor}` (eg. 'unknown'), `{system}` (eg. 'darwin'), `{cfg}` (eg. 'exec'), `{channel}` (eg. 'stable'), and `{tool}` (eg. 'rustc.exe') will be replaced in the string if present.   | String | optional |  `"@rust_{system}_{arch}__{triple}__{channel}_tools//:bin/{tool}"`  |
| <a id="crates_repository-rust_toolchain_rustc_template"></a>rust_toolchain_rustc_template |  The template to use for finding the host `rustc` binary. `{version}` (eg. '1.53.0'), `{triple}` (eg. 'x86_64-unknown-linux-gnu'), `{arch}` (eg. 'aarch64'), `{vendor}` (eg. 'unknown'), `{system}` (eg. 'darwin'), `{cfg}` (eg. 'exec'), `{channel}` (eg. 'stable'), and `{tool}` (eg. 'cargo.exe') will be replaced in the string if present.   | String | optional |  `"@rust_{system}_{arch}__{triple}__{channel}_tools//:bin/{tool}"`  |
| <a id="crates_repository-rust_version"></a>rust_version |  The version of Rust the currently registered toolchain is using. Eg. `1.56.0`, or `nightly/2021-09-08`   | String | optional |  `"1.83.0"`  |
| <a id="crates_repository-splicing_config"></a>splicing_config |  The configuration flags to use for splicing Cargo maniests. Use `//crate_universe:defs.bzl\%rsplicing_config` to generate the value for this field. If unset, the defaults defined there will be used.   | String | optional |  `""`  |
| <a id="crates_repository-supported_platform_triples"></a>supported_platform_triples |  A set of all platform triples to consider when generating dependencies.   | List of strings | optional |  `["aarch64-apple-darwin", "aarch64-apple-ios", "aarch64-apple-ios-sim", "aarch64-linux-android", "aarch64-pc-windows-msvc", "aarch64-unknown-fuchsia", "aarch64-unknown-linux-gnu", "aarch64-unknown-nixos-gnu", "aarch64-unknown-nto-qnx710", "arm-unknown-linux-gnueabi", "armv7-linux-androideabi", "armv7-unknown-linux-gnueabi", "i686-apple-darwin", "i686-linux-android", "i686-pc-windows-msvc", "i686-unknown-freebsd", "i686-unknown-linux-gnu", "powerpc-unknown-linux-gnu", "riscv32imc-unknown-none-elf", "riscv64gc-unknown-none-elf", "s390x-unknown-linux-gnu", "thumbv7em-none-eabi", "thumbv8m.main-none-eabi", "wasm32-unknown-unknown", "wasm32-wasip1", "x86_64-apple-darwin", "x86_64-apple-ios", "x86_64-linux-android", "x86_64-pc-windows-msvc", "x86_64-unknown-freebsd", "x86_64-unknown-fuchsia", "x86_64-unknown-linux-gnu", "x86_64-unknown-nixos-gnu", "x86_64-unknown-none"]`  |

**ENVIRONMENT VARIABLES**

This repository rule depends on the following environment variables:
* `CARGO_BAZEL_GENERATOR_URL`
* `CARGO_BAZEL_GENERATOR_SHA256`
* `CARGO_BAZEL_REPIN`
* `REPIN`
* `CARGO_BAZEL_REPIN_ONLY`
* `CARGO_BAZEL_ISOLATED`
* `CARGO_BAZEL_DEBUG`


