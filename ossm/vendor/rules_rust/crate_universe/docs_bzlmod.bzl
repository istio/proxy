"""
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
In situations like this, it may be desirable to have a “Cargo free” setup. You find an example in the in the [example folder](../examples/bzlmod/hello_world_no_cargo).

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
    ├── 3rdparty
    │   ├── BUILD.bazel
    │   ├── Cargo.Bazel.lock
```

Now you can run the `crates_vendor` target:

`bazel run //basic/3rdparty:crates_vendor`

This generates a crate folders with all configurations for the vendored dependencies.

```
basic
    ├── 3rdparty
    │   ├── cratea
    │   ├── BUILD.bazel
    │   ├── Cargo.Bazel.lock
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

"""

load(
    "//crate_universe:extensions.bzl",
    _crate = "crate",
)

crate = _crate
