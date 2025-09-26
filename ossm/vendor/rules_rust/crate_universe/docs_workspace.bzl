"""# Crate Universe

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
"""

load(
    "//crate_universe:defs.bzl",
    _crate = "crate",
    _crates_repository = "crates_repository",
    _crates_vendor = "crates_vendor",
    _render_config = "render_config",
    _splicing_config = "splicing_config",
)
load(
    "//crate_universe:repositories.bzl",
    _crate_universe_dependencies = "crate_universe_dependencies",
)
load(
    "//crate_universe/3rdparty/crates:defs.bzl",
    _aliases = "aliases",
    _all_crate_deps = "all_crate_deps",
    _crate_deps = "crate_deps",
    _crate_repositories = "crate_repositories",
)

# Rules
crates_repository = _crates_repository
crates_vendor = _crates_vendor

# Utility Macros
crate_universe_dependencies = _crate_universe_dependencies
crate = _crate
render_config = _render_config
splicing_config = _splicing_config

# Dependencies API
aliases = _aliases
all_crate_deps = _all_crate_deps
crate_deps = _crate_deps
crate_repositories = _crate_repositories
