"""# Crate Universe

Crate Universe is a set of Bazel rule for generating Rust targets using Cargo.

This doc describes using crate_universe with bzlmod.

If you're using a WORKSPACE file, please see [the WORKSPACE equivalent of this doc](crate_universe_workspace.html).

There are some examples of using crate_universe with bzlmod in the [example folder](https://github.com/bazelbuild/rules_rust/tree/main/examples).

# Table of Contents

1. [Setup](#Setup)
2. [Dependencies](#dependencies)
    * [Cargo Workspace](#cargo-workspaces)
    * [Direct Packages](#direct-dependencies)
    * [Binary Dependencies](#binary-dependencies)
    * [Vendored Dependencies](#vendored-dependencies)
3. [Crate reference](#crate)
   * [from_cargo](#from_cargo)
   * [from_specs](#from_specs)


## Setup

To use rules_rust in a project using bzlmod, add the following to your MODULE.bazel file:

```python
bazel_dep(name = "rules_rust", version = "0.69.0")
```

You find the latest version on the [release page](https://github.com/bazelbuild/rules_rust/releases).


After adding `rules_rust` in your `MODULE.bazel`, set the following to begin using `crate_universe`:

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")
//  # ... Dependencies
use_repo(crate, "crates")
```

## Dependencies

There are three different ways to declare dependencies in your MODULE.

1) Cargo workspace
2) Direct Dependencies
3) Binary Dependencies
4) Vendored Dependencies

### Cargo Workspaces

One of the simpler ways to wire up dependencies would be to first structure your project into a Cargo workspace.
The crates_repository rule can ingest a root Cargo.toml file and generate Bazel dependencies from there.
You find a complete example in the in the [example folder](https://github.com/bazelbuild/rules_rust/tree/main/examples/all_crate_deps).

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")

crate.from_cargo(
    name = "crates",
    cargo_lockfile = "//:Cargo.lock",
    manifests = ["//:Cargo.toml"],
)
use_repo(crate, "crates")
```

#### Note if using Private Crate Registries

If you are using from_cargo and are pulling dependencies from a private crate registry such as Artifactory,
make sure you set the `CARGO_BAZEL_ISOLATED=false bazel build //...` environmental.  If not `crates_universe`
will not be able to pull from your private registry.

The generated crates_repository contains helper macros which make collecting dependencies for Bazel targets simpler.
Notably, the all_crate_deps and aliases macros (
see [Dependencies API](https://bazelbuild.github.io/rules_rust/crate_universe_workspace.html#dependencies-api)) commonly allow the
Cargo.toml files to be the single source of truth for dependencies.
Since these macros come from the generated repository, the dependencies and alias definitions
they return will automatically update BUILD targets. In your BUILD files,
you use these macros for a Rust library as shown below:

```python
load("@crates//:defs.bzl", "aliases", "all_crate_deps")
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
For more details about repin, [please refer to the documentation](https://bazelbuild.github.io/rules_rust/crate_universe_workspace.html#crates_vendor).

### Direct Dependencies

In cases where Rust targets have heavy interactions with other Bazel targets ([Cc](https://docs.bazel.build/versions/main/be/c-cpp.html), [Proto](https://rules-proto-grpc.com/en/4.5.0/lang/rust.html),
etc.), maintaining Cargo.toml files may have diminishing returns as things like rust-analyzer
begin to be confused about missing targets or environment variables defined only in Bazel.
In situations like this, it may be desirable to have a "Cargo free" setup. You find an example in the in the [example folder](https://github.com/bazelbuild/rules_rust/examples/bzlmod/hello_world_no_cargo).

crates_repository supports this through the packages attribute,
as shown below.

```python
crate = use_extension("@rules_rust//crate_universe:extensions.bzl", "crate")

crate.spec(package = "serde", features = ["derive"], version = "1.0")
crate.spec(package = "serde_json", version = "1.0")
crate.spec(package = "tokio", default_features = False, features = ["macros", "net", "rt-multi-thread"], version = "1.38")

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

### Binary Dependencies

With cargo you `can install` binary dependencies (bindeps) as well with `cargo install` command.

We don't have such easy facilities available in bazel besides specifying it as a dependency.
To mimic cargo's bindeps feature we use the unstable feature called [artifact-dependencies](https://doc.rust-lang.org/nightly/cargo/reference/unstable.html?highlight=feature#artifact-dependencies)
which integrates well with bazel concepts.

You could use the syntax specified in the above document to place it in `Cargo.toml`. For that you can consult the following [example](https://github.com/bazelbuild/rules_rust/blob/main/examples/crate_universe/MODULE.bazel#L279-L291).

This method has the following consequences:
* if you use shared dependency tree with your project these binary dependencies will interfere with yours (may conflict)
* you have to use  nightly `host_tools` to generate dependencies because

Alternatively you can specify this in a separate `repo` with `cargo.from_specs` syntax:

```python
bindeps = use_extension("@rules_rust//crate_universe:extension.bzl", "crate")

bindeps.spec(package = "cargo-machete", version = "=0.7.0", artifact = "bin")
bindeps.annotation(crate = "cargo-machete", gen_all_binaries = True)

bindeps.from_specs(
  name = "bindeps",
  host_tools = "@rust_host_tools_nightly",
)

use_repo(bindeps, "bindeps")
```

You can run the specified binary dependency with the following command or create additional more complex rules on top of it.

```bash
bazel run @bindeps//:cargo-machete__cargo-machete
```

Notice, direct dependencies do not need repining.
Only a cargo workspace needs updating whenever the underlying Cargo.toml file changed.

### Vendored Dependencies

In some cases, it is require that all external dependencies are vendored, meaning downloaded
and stored in the workspace. This helps, for example, to conduct licence scans, apply custom patches,
or to ensure full build reproducibility since no download error could possibly occur.
You find a complete example in the in the [example folder](https://github.com/bazelbuild/rules_rust/examples/bzlmod/all_deps_vendor).

For the setup, you need to add the skylib in addition to the rust rules to your `MODULE.bazel`.

```python
module(
    name = "deps_vendored",
    version = "0.0.0"
)
###############################################################################
# B A Z E L  C E N T R A L  R E G I S T R Y # https://registry.bazel.build/
###############################################################################
# https://github.com/bazelbuild/bazel-skylib/releases/
bazel_dep(name = "bazel_skylib", version = "1.8.2")

# https://github.com/bazelbuild/rules_rust/releases
bazel_dep(name = "rules_rust", version = "0.69.0")

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
    |-- 3rdparty
    |   |-- BUILD.bazel
    |   |-- Cargo.Bazel.lock
```

Now you can run the `crates_vendor` target:

`bazel run //basic/3rdparty:crates_vendor`

This generates a crate folders with all configurations for the vendored dependencies.

```
basic
    |-- 3rdparty
    |   |-- cratea
    |   |-- BUILD.bazel
    |   |-- Cargo.Bazel.lock
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

load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:structs.bzl", "structs")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    "//crate_universe/private:common_utils.bzl",
    "new_cargo_bazel_fn",
)
load("//crate_universe/private:crates_repository.bzl", "SUPPORTED_PLATFORM_TRIPLES")
load(
    "//crate_universe/private:crates_vendor.bzl",
    "CRATES_VENDOR_ATTRS",
    "generate_config_file",
    "generate_splicing_manifest",
)
load(
    "//crate_universe/private:generate_utils.bzl",
    "CARGO_BAZEL_GENERATOR_SHA256",
    "CARGO_BAZEL_GENERATOR_URL",
    "CRATES_REPOSITORY_ENVIRON",
    "GENERATOR_ENV_VARS",
    "determine_repin",
    "execute_generator",
    generate_render_config = "render_config",
)
load("//crate_universe/private:local_crate_mirror.bzl", "local_crate_mirror")
load(
    "//crate_universe/private:splicing_utils.bzl",
    "splice_workspace_manifest",
    generate_splicing_config = "splicing_config",
)
load("//crate_universe/private:urls.bzl", "CARGO_BAZEL_SHA256S", "CARGO_BAZEL_URLS")
load("//rust/platform:triple.bzl", "get_host_triple")
load("//rust/platform:triple_mappings.bzl", "system_to_binary_ext")
load(":defs.bzl", _crate_universe_crate = "crate")

# A list of labels which may be relative (and if so, is within the repo the rule is generated in).
#
# If I were to write ":foo", with attr.label_list, it would evaluate to
# "@@//:foo". However, for a tag such as deps, ":foo" should refer to
# "@@rules_rust~crates~<crate>//:foo".
_relative_label_list = attr.string_list

_OPT_BOOL_VALUES = {
    "auto": None,
    "off": False,
    "on": True,
}

def _get_or_insert(d, key, value):
    if key not in d:
        d[key] = value
    return d[key]

def _generate_repo_impl(repository_ctx):
    for path, contents in repository_ctx.attr.contents.items():
        repository_ctx.file(path, contents)
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

_generate_repo = repository_rule(
    doc = "A utility for generating a hub repo.",
    implementation = _generate_repo_impl,
    attrs = {
        "contents": attr.string_dict(
            doc = "A mapping of file names to text they should contain.",
            mandatory = True,
        ),
    },
)

def _annotations_for_repo(module_annotations, repo_specific_annotations):
    """Merges the set of global annotations with the repo-specific ones

    Args:
        module_annotations (dict): The annotation tags that apply to all repos, keyed by crate.
        repo_specific_annotations (dict): The annotation tags that apply to only this repo, keyed by crate.
    """

    if not repo_specific_annotations:
        return module_annotations

    annotations = dict(module_annotations)
    for crate, values in repo_specific_annotations.items():
        _get_or_insert(annotations, crate, []).extend(values)
    return annotations

def _collect_render_config(module, repository):
    """Collect the render_config for the given crate_universe module.

    Args:
        module (StarlarkBazelModule): The current `crate` module.
        repository (str): The name of the repository to collect the config for.

    Returns:
        dict: The rendering config to use.
    """

    config = None
    for raw_config in module.tags.render_config:
        # if the repositories is empty we apply the config to all repositories
        # otherwise we filter for the requested repositories
        if raw_config.repositories and repository not in raw_config.repositories:
            continue

        if config:
            fail("Multiple render configs provided for module `{}`. Only 1 is allowed.".format(
                module.name,
            ))

        config_kwargs = {attr: getattr(raw_config, attr) for attr in dir(raw_config)}

        if "repositories" in config_kwargs:
            config_kwargs.pop("repositories")

        if "default_alias_rule_bzl" in config_kwargs:
            default_alias_rule_bzl = config_kwargs.pop("default_alias_rule_bzl")
            if default_alias_rule_bzl:
                config_kwargs["default_alias_rule"] = str(default_alias_rule_bzl)

        if "default_alias_rule_name" in config_kwargs:
            config_kwargs["default_alias_rule"] = "{}:{}".format(
                config_kwargs.get("default_alias_rule", ""),
                config_kwargs.pop("default_alias_rule_name"),
            ).lstrip(":")

        # bzlmod doesn't allow passing `None` as a default parameter to indicate a value was
        # not provided. So for backward compatibility, certain empty values are assumed to be
        # not provided and thus are converted explicitly to `None`.
        for null_defaults in ["vendor_mode", "regen_command", "default_package_name"]:
            if config_kwargs[null_defaults] == "":
                config_kwargs[null_defaults] = None

        config = json.decode(generate_render_config(**config_kwargs))

    if not config:
        config = json.decode(generate_render_config())

    if not config["regen_command"]:
        config["regen_command"] = "bazel mod show_repo '{}'".format(module.name)

    return config

def _collect_splicing_config(module, repository):
    """Collect the splicing_config for the given crate_universe module.

    Args:
        module (StarlarkBazelModule): The current `crate` module.
        repository (str): The name of the repository to collect the config for.

    Returns:
        dict: The splicing config to use.
    """
    config = None
    for raw_config in module.tags.splicing_config:
        # if the repositories is empty we apply the config to all repositories
        # otherwise we filter for the requested repositories
        if raw_config.repositories and repository not in raw_config.repositories:
            continue

        if config:
            fail("Multiple render configs provided for module `{}`. Only 1 is allowed.".format(
                module.name,
            ))

        config_kwargs = {attr: getattr(raw_config, attr) for attr in dir(raw_config)}

        if "repositories" in config_kwargs:
            config_kwargs.pop("repositories")

        config = json.decode(generate_splicing_config(**config_kwargs))

    if not config:
        config = json.decode(generate_splicing_config())

    return config

def _generate_hub_and_spokes(
        *,
        module_ctx,
        cargo_bazel_fn,
        cfg,
        annotations,
        render_config,
        splicing_config,
        lockfile,
        skip_cargo_lockfile_overwrite,
        strip_internal_dependencies_from_cargo_lockfile,
        cargo_lockfile = None,
        manifests = {},
        packages = {}):
    """Generates repositories for the transitive closure of crates defined by manifests and packages.

    Args:
        module_ctx (module_ctx): The module context object.
        cargo_bazel_fn (callable): A callback for invoking the `cargo-bazel` binary.
        cfg (object): The module tag from `from_cargo` or `from_specs`
        annotations (dict): The set of annotation tag classes that apply to this closure, keyed by crate name.
        render_config (dict): The render config to use.
        splicing_config (dict): The splicing config to use.
        lockfile (path): The path to the crate_universe lock file, if one was provided.
        skip_cargo_lockfile_overwrite (bool): Whether to skip writing the cargo lockfile back after resolving.
            You may want to set this if your dependency versions are maintained externally through a non-trivial set-up.
            But you probably don't want to set this.
        strip_internal_dependencies_from_cargo_lockfile (bool): Whether to strip internal dependencies from the cargo lockfile.
            You may want to use this if you want to maintain a cargo lockfile for bazel only.
            Bazel only requires external dependencies to be present in the lockfile.
            By removing internal dependencies, the lockfile changes less frequently which reduces merge conflicts
            in other lockfiles where the cargo lockfile's sha is stored.
        cargo_lockfile (path): Path to Cargo.lock, if we have one.
        manifests (dict): The set of Cargo.toml manifests that apply to this closure, if any, keyed by path.
        packages (dict): The set of extra cargo crate tags that apply to this closure, if any, keyed by package name.
    """

    tag_path = module_ctx.path(cfg.name)

    config_file = tag_path.get_child("config.json")
    module_ctx.file(
        config_file,
        executable = False,
        content = generate_config_file(
            module_ctx,
            mode = "remote",
            annotations = annotations,
            generate_build_scripts = cfg.generate_build_scripts,
            supported_platform_triples = cfg.supported_platform_triples,
            generate_target_compatible_with = True,
            repository_name = cfg.name,
            output_pkg = cfg.name,
            workspace_name = cfg.name,
            generate_binaries = cfg.generate_binaries,
            render_config = render_config,
            repository_ctx = module_ctx,
        ),
    )

    splicing_manifest = tag_path.get_child("splicing_manifest.json")
    module_ctx.file(
        splicing_manifest,
        executable = False,
        content = generate_splicing_manifest(
            packages = packages,
            splicing_config = splicing_config,
            cargo_config = cfg.cargo_config,
            manifests = manifests,
            manifest_to_path = module_ctx.path,
        ),
    )

    # TODO: Repins should never be allowed if the lockfile is not within
    # https://github.com/bazelbuild/rules_rust/issues/1738

    # Determine whether or not to repin dependencies
    repin = not lockfile or determine_repin(
        repository_ctx = module_ctx,
        repository_name = cfg.name,
        cargo_bazel_fn = cargo_bazel_fn,
        lockfile_path = lockfile,
        config = config_file,
        splicing_manifest = splicing_manifest,
    )

    # The workspace root when one is explicitly provided.
    nonhermetic_root_bazel_workspace_dir = module_ctx.path(Label("@@//:MODULE.bazel")).dirname

    # If re-pinning is enabled, gather additional inputs for the generator
    kwargs = dict()
    if repin:
        module_ctx.report_progress("Splicing Cargo workspace for `{}`".format(cfg.name))

        # Generate a top level Cargo workspace and manifest for use in generation
        splice_outputs = splice_workspace_manifest(
            repository_ctx = module_ctx,
            cargo_bazel_fn = cargo_bazel_fn,
            cargo_lockfile = cargo_lockfile,
            splicing_manifest = splicing_manifest,
            config_path = config_file,
            output_dir = tag_path.get_child("splicing-output"),
            debug_workspace_dir = tag_path.get_child("splicing-workspace"),
            skip_cargo_lockfile_overwrite = cfg.skip_cargo_lockfile_overwrite,
            nonhermetic_root_bazel_workspace_dir = nonhermetic_root_bazel_workspace_dir,
            repository_name = cfg.name,
        )

        # If a cargo lockfile was not provided, use the splicing lockfile.
        if cargo_lockfile == None:
            cargo_lockfile = splice_outputs.cargo_lock

        # Create a fallback lockfile to be parsed downstream.
        if lockfile == None:
            lockfile = tag_path.get_child("cargo-bazel-lock.json")
            module_ctx.file(lockfile, "")

        kwargs.update({
            "metadata": splice_outputs.metadata,
        })

        for path_to_track in splice_outputs.extra_paths_to_track:
            # We can only watch paths in our workspace.
            if path_to_track.startswith(str(nonhermetic_root_bazel_workspace_dir)):
                module_ctx.watch(path_to_track)

    paths_to_track_file = tag_path.get_child("paths_to_track.json")
    warnings_output_file = tag_path.get_child("warnings_output.json")

    # Run the generator
    module_ctx.report_progress("Generating crate BUILD files for `{}`".format(cfg.name))
    execute_generator(
        cargo_bazel_fn = cargo_bazel_fn,
        config = config_file,
        splicing_manifest = splicing_manifest,
        lockfile_path = lockfile,
        cargo_lockfile_path = cargo_lockfile,
        repository_dir = tag_path,
        nonhermetic_root_bazel_workspace_dir = nonhermetic_root_bazel_workspace_dir,
        paths_to_track_file = paths_to_track_file,
        warnings_output_file = warnings_output_file,
        skip_cargo_lockfile_overwrite = skip_cargo_lockfile_overwrite,
        strip_internal_dependencies_from_cargo_lockfile = strip_internal_dependencies_from_cargo_lockfile,
        **kwargs
    )

    module_ctx.report_progress("Generating hub and spokes")

    paths_to_track = json.decode(module_ctx.read(paths_to_track_file))
    for path in paths_to_track:
        module_ctx.watch(path)

    warnings_output_file = json.decode(module_ctx.read(warnings_output_file))
    for warning in warnings_output_file:
        # buildifier: disable=print
        print("WARN: {}".format(warning))

    crates_dir = tag_path.get_child(cfg.name)
    _generate_repo(
        name = cfg.name,
        contents = {
            "BUILD.bazel": module_ctx.read(crates_dir.get_child("BUILD.bazel")),
            "alias_rules.bzl": module_ctx.read(crates_dir.get_child("alias_rules.bzl")),
            "defs.bzl": module_ctx.read(crates_dir.get_child("defs.bzl")),
        },
    )

    contents = json.decode(module_ctx.read(lockfile))

    for crate in contents["crates"].values():
        repo = crate["repository"]
        if repo == None:
            continue
        name = crate["name"]
        version = crate["version"]

        # "+" isn't valid in a repo name.
        crate_repo_name = "{repo_name}__{name}-{version}".format(
            repo_name = cfg.name,
            name = name,
            version = version.replace("+", "-"),
        )

        if "Http" in repo:
            # Replicates functionality in repo_http.j2.
            build_file_content = module_ctx.read(crates_dir.get_child("BUILD.%s-%s.bazel" % (name, version)))
            repo = repo["Http"]
            http_archive(
                name = crate_repo_name,
                patch_args = repo.get("patch_args", None),
                patch_tool = repo.get("patch_tool", None),
                patches = repo.get("patches", None),
                remote_patch_strip = 1,
                sha256 = repo.get("sha256", None),
                type = "tar.gz",
                urls = [repo["url"]],
                strip_prefix = "%s-%s" % (crate["name"], crate["version"]),
                build_file_content = build_file_content,
            )
        elif "Git" in repo:
            # Replicates functionality in repo_git.j2
            build_file_content = module_ctx.read(crates_dir.get_child("BUILD.%s-%s.bazel" % (name, version)))
            repo = repo["Git"]
            kwargs = {}
            for k, v in repo["commitish"].items():
                if k == "Rev":
                    kwargs["commit"] = v
                else:
                    kwargs[k.lower()] = v
            new_git_repository(
                name = crate_repo_name,
                init_submodules = True,
                patch_args = repo.get("patch_args", None),
                patch_tool = repo.get("patch_tool", None),
                patches = repo.get("patches", None),
                shallow_since = repo.get("shallow_since", None),
                remote = repo["remote"],
                build_file_content = build_file_content,
                strip_prefix = repo.get("strip_prefix", None),
                **kwargs
            )
        elif "Path" in repo:
            options = {
                "config": render_config,
                "crate_context": crate,
                "platform_conditions": contents["conditions"],
                "supported_platform_triples": cfg.supported_platform_triples,
            }
            kwargs = {}
            if len(CARGO_BAZEL_URLS) == 0:
                kwargs["generator"] = "@cargo_bazel_bootstrap//:cargo-bazel"
            local_crate_mirror(
                name = crate_repo_name,
                options_json = json.encode(options),
                path = repo["Path"]["path"],
                **kwargs
            )
        else:
            fail("Invalid repo: expected Http or Git to exist for crate %s-%s, got %s" % (name, version, repo))

def _package_to_json(p):
    # Avoid adding unspecified properties.
    # If we add them as empty strings, cargo-bazel will be unhappy.
    return json.encode({
        k: v
        for k, v in structs.to_dict(p).items()
        if v or k == "default_features"
    })

def _get_generator(module_ctx, host_triple):
    """Query Network Resources to local a `cargo-bazel` binary.

    Based off get_generator in crates_universe/private/generate_utils.bzl

    Args:
        module_ctx (module_ctx): The rules context object
        host_triple (struct): A triple struct that represents the host.

    Returns:
        tuple(path, dict) The path to a 'cargo-bazel' binary. The pairing (dict)
            may be `None` if there is not need to update the attribute
    """
    use_environ = False
    for var in GENERATOR_ENV_VARS:
        if var in module_ctx.os.environ:
            use_environ = True

    if use_environ:
        generator_sha256 = module_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_SHA256)
        generator_url = module_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_URL)
    elif len(CARGO_BAZEL_URLS) == 0:
        # For Bazel 7 and below, `module_ctx.path` will also watch files so to avoid
        # volatility in lock files caused by referencing host specific files, direct
        # references are avoided.
        return module_ctx.path(Label("@cargo_bazel_bootstrap//:BUILD.bazel")).dirname.get_child("cargo-bazel")

    else:
        generator_sha256 = CARGO_BAZEL_SHA256S.get(host_triple.str)
        generator_url = CARGO_BAZEL_URLS.get(host_triple.str)

    if not generator_url:
        fail((
            "No generator URL was found either in the `CARGO_BAZEL_GENERATOR_URL` " +
            "environment variable or for the `{}` triple in the `generator_urls` attribute"
        ).format(host_triple.str))

    output = module_ctx.path("cargo-bazel.exe" if "win" in module_ctx.os.name else "cargo-bazel")

    # Download the file into place
    download_kwargs = {
        "executable": True,
        "output": output,
        "url": generator_url,
    }

    if generator_sha256:
        download_kwargs.update({"sha256": generator_sha256})

    module_ctx.download(**download_kwargs)
    return output

def _get_host_cargo_rustc(module_ctx, host_triple, host_tools_repo):
    """A helper function to get the path to the host cargo and rustc binaries.

    Args:
        module_ctx (module_ctx): The module extension's context.
        host_triple (triple): The platform triple for the machine executing this extension.
        host_tools_repo (Label): The `rust_host_tools` repository to use.
    Returns:
        tuple[Path, Path]: A tuple of path to cargo, path to rustc.
    """
    binary_ext = system_to_binary_ext(host_triple.system)
    root = module_ctx.path(host_tools_repo)

    cargo_path = root.dirname.get_child("bin/cargo{}".format(binary_ext))
    rustc_path = root.dirname.get_child("bin/rustc{}".format(binary_ext))

    return cargo_path, rustc_path

def _update_annotations(store, crate, version, triples, annotation_select_dict):
    """Insert build-script data/env for each target triple if keys exist."""
    crate_info = store.get(crate)
    if crate_info == None:
        _insert_annotation(store, crate, version, {})
        crate_info = store.get(crate)

    version_info = crate_info.get(version)
    if version_info == None:
        _insert_annotation(store, crate, version, {})
        version_info = crate_info.get(version)

    filtered_annotation_select_dict = _filter_dict(annotation_select_dict)

    for triple in triples:
        for field, value in filtered_annotation_select_dict.items():
            _insert_select(
                crate,
                version,
                triple,
                version_info,
                field,
                value,
            )

def _insert_annotation(store, crate, version, annotation):
    """Insert `annotation` (dict) into `store[crate][version]`.

    Raises `fail()` if that (crate, version) pair already exists.
    """
    crate_map = store.setdefault(crate, {})  # create nested map if needed

    if version in crate_map:
        fail("Duplicate annotation for crate {} version {}".format(crate, version))

    crate_map[version] = annotation

def _insert_select(crate, version, triple, annotation, field, value):
    """
    Add a per-triple override to annotation[field].

    If annotation[field] is in a list or dict, we wrap that into the common field. Then

    On later calls we just update the existing selects.
    """
    current = annotation.get(field)

    if type(current) == "struct":
        if triple in current.selects:
            fail("Duplicate annotation for crate {} version {} triple {}".format(crate, version, triple))

        current.selects.update({triple: value})
        return

    # `common` cannot be None
    if current == None:
        if type(value) == type(""):
            current = ""
        elif type(value) == type([]):
            current = []
        elif type(value) == type({}):
            current = {}

    annotation[field] = struct(
        common = current,
        selects = {triple: value},
    )

def _create_annotation(version, annotation_dict):
    return _crate_universe_crate.annotation(
        version = version,
        **_filter_dict(annotation_dict)
    )

def _collapse_crate_versions(crates, create = _create_annotation):
    """{crate: {version: annotation}} -> {crate: [annotation,…]}"""
    return {
        crate: [create(version, annotation) for version, annotation in versions.items()]
        for crate, versions in crates.items()
    }

def _collapse_versions(repo_specific, module, create = _create_annotation):
    """
    Return new (repo_specific, module) dicts with versions collapsed.

    repo_specific: {repo: {crate: {version: annotation}}}
    module       : {crate: {version: annotation}}
    """
    new_repo_specific = {
        repo: _collapse_crate_versions(crates, create)
        for repo, crates in repo_specific.items()
    }
    new_module = _collapse_crate_versions(module, create)
    return new_repo_specific, new_module

def _filter_dict(dict):
    return {
        k: v
        for k, v in dict.items()
        # Tag classes can't take in None, but the function requires None
        # instead of the empty values in many cases.
        # https://github.com/bazelbuild/bazel/issues/20744
        if v != "" and v != [] and v != {}
    }

def _crate_impl(module_ctx):
    reproducible = True
    host_triple = get_host_triple(module_ctx, abi = {
        "aarch64-unknown-linux": "musl",
        "x86_64-unknown-linux": "musl",
    })
    generator = _get_generator(module_ctx, host_triple)

    all_repos = []

    for mod in module_ctx.modules:
        if not mod.tags.from_cargo and not mod.tags.from_specs:
            fail("`.from_cargo` or `.from_specs` are required. Please update {}".format(mod.name))

        local_repos = []
        module_annotations = {}
        repo_specific_annotations = {}
        for annotation_tag in mod.tags.annotation:
            annotation_dict = structs.to_dict(annotation_tag)
            if "build_script_data_select" in annotation_dict:
                annotation_dict["build_script_data"] = struct(
                    common = annotation_dict["build_script_data"],
                    selects = annotation_dict["build_script_data_select"],
                )
                annotation_dict.pop("build_script_data_select")
            if "build_script_env_select" in annotation_dict:
                annotation_dict["build_script_env"] = struct(
                    common = annotation_dict["build_script_env"],
                    selects = {
                        k: json.decode(v)
                        for k, v in annotation_dict["build_script_env_select"].items()
                    },
                )
                annotation_dict.pop("build_script_env_select")

            repositories = annotation_dict.pop("repositories")
            crate = annotation_dict.pop("crate")
            version = annotation_dict.pop("version")

            # The crate.annotation function can take in either a list or a bool.
            # For the tag-based method, because it has type safety, we have to
            # split it into two parameters.
            if annotation_dict.pop("gen_all_binaries"):
                annotation_dict["gen_binaries"] = True
            annotation_dict["gen_build_script"] = _OPT_BOOL_VALUES[annotation_dict["gen_build_script"]]

            # Process the override targets for the annotation.
            # In the non-bzlmod approach, this is given as a dict
            # with the possible keys "`proc_macro`, `build_script`, `lib`, `bin`".
            # With the tag-based approach used in Bzlmod, we run into an issue
            # where there is no dict type that takes a string as a key and a Label as the value.
            # To work around this, we split the override option into four, and reconstruct the
            # dictionary here during processing
            annotation_dict["override_targets"] = dict()
            replacement = annotation_dict.pop("override_target_lib")
            if replacement:
                annotation_dict["override_targets"]["lib"] = str(replacement)

            replacement = annotation_dict.pop("override_target_proc_macro")
            if replacement:
                annotation_dict["override_targets"]["proc_macro"] = str(replacement)

            replacement = annotation_dict.pop("override_target_build_script")
            if replacement:
                annotation_dict["override_targets"]["build_script"] = str(replacement)

            replacement = annotation_dict.pop("override_target_bin")
            if replacement:
                annotation_dict["override_targets"]["bin"] = str(replacement)

            if not repositories:
                _insert_annotation(module_annotations, crate, version, annotation_dict)
            else:
                for repo in repositories:
                    # each repo gets its own top-level dict:
                    repo_map = repo_specific_annotations.setdefault(repo, {})
                    _insert_annotation(repo_map, crate, version, annotation_dict)

        for annotation_select_tag in mod.tags.annotation_select:
            annotation_select_dict = structs.to_dict(annotation_select_tag)
            repositories = annotation_select_dict.pop("repositories")
            crate = annotation_select_dict.pop("crate")
            triples = annotation_select_dict.pop("triples")
            version = annotation_select_dict.pop("version")

            if repositories:
                for repo in repositories:
                    _update_annotations(
                        repo_specific_annotations.setdefault(repo, {}),
                        crate,
                        version,
                        triples,
                        annotation_select_dict,
                    )
            else:
                _update_annotations(
                    module_annotations,
                    crate,
                    version,
                    triples,
                    annotation_select_dict,
                )

        # Now transform the annotations into a format that the `_generate_hub_and_spokes` function expects.
        repo_specific_annotations, module_annotations = _collapse_versions(repo_specific_annotations, module_annotations)
        common_specs = []
        repo_specific_specs = {}
        for spec in mod.tags.spec:
            if not spec.repositories:
                common_specs.append(spec)
            else:
                for name in spec.repositories:
                    if name not in repo_specific_specs:
                        repo_specific_specs[name] = []
                    repo_specific_specs[name].append(spec)

        # Do a quick sanity check to ensure no annotations or specs are referring to
        # non-existent modules.
        for cfg in mod.tags.from_cargo + mod.tags.from_specs:
            if cfg.name in local_repos:
                fail("Defined two crate universes with the same name in the same MODULE.bazel file (`{}`). Use the `name` tag to give them different names.".format(
                    cfg.name,
                ))
            if cfg.name in all_repos:
                fail("Defined two crate universes with the same name in different MODULE.bazel files (`{}`). Either give one a different name, or use `use_extension(isolate=True)`".format(
                    cfg.name,
                ))
            all_repos.append(cfg.name)
            local_repos.append(cfg.name)

        for repo in repo_specific_annotations:
            if repo not in local_repos:
                fail("Annotation specified for repo {}, but the module defined repositories {}".format(repo, local_repos))

        for repo in repo_specific_specs:
            if repo not in local_repos:
                fail("Spec specified for repo {}, but the module defined repositories {}".format(repo, local_repos))

        for cfg in mod.tags.from_cargo + mod.tags.from_specs:
            # Preload all external repositories. Calling `module_ctx.watch` will cause restarts of the implementation
            # function of the module extension when the file has changed.
            if cfg.cargo_lockfile:
                module_ctx.watch(cfg.cargo_lockfile)
            if cfg.lockfile:
                module_ctx.watch(cfg.lockfile)
            if cfg.cargo_config:
                module_ctx.watch(cfg.cargo_config)
            if hasattr(cfg, "manifests"):
                for m in cfg.manifests:
                    module_ctx.watch(m)

            cargo_path, rustc_path = _get_host_cargo_rustc(module_ctx, host_triple, cfg.host_tools)
            cargo_bazel_fn = new_cargo_bazel_fn(
                repository_ctx = module_ctx,
                cargo_bazel_path = generator,
                cargo_path = cargo_path,
                rustc_path = rustc_path,
                isolated = cfg.isolated,
            )

            rendering_config = _collect_render_config(mod, cfg.name)
            splicing_config = _collect_splicing_config(mod, cfg.name)

            annotations = _annotations_for_repo(
                module_annotations,
                repo_specific_annotations.get(cfg.name),
            )

            lockfile_path = None
            if cfg.lockfile:
                lockfile_path = module_ctx.path(cfg.lockfile)
            else:
                reproducible = False

            cargo_lockfile = None
            if cfg.cargo_lockfile:
                cargo_lockfile = module_ctx.path(cfg.cargo_lockfile)
            else:
                reproducible = False

            manifests = {}
            packages = {}

            # Only `from_cargo` instances will have `manifests`.
            if hasattr(cfg, "manifests"):
                manifests = {str(module_ctx.path(m)): str(m) for m in cfg.manifests}

            packages = {
                p.package: _package_to_json(p)
                for p in common_specs + repo_specific_specs.get(cfg.name, [])
            }

            _generate_hub_and_spokes(
                module_ctx = module_ctx,
                cargo_bazel_fn = cargo_bazel_fn,
                cfg = cfg,
                annotations = annotations,
                lockfile = lockfile_path,
                cargo_lockfile = cargo_lockfile,
                render_config = rendering_config,
                splicing_config = splicing_config,
                manifests = manifests,
                packages = packages,
                skip_cargo_lockfile_overwrite = cfg.skip_cargo_lockfile_overwrite,
                strip_internal_dependencies_from_cargo_lockfile = cfg.strip_internal_dependencies_from_cargo_lockfile,
            )

    metadata_kwargs = {}
    if bazel_features.external_deps.extension_metadata_has_reproducible:
        metadata_kwargs["reproducible"] = reproducible

    return module_ctx.extension_metadata(**metadata_kwargs)

_FROM_COMMON_ATTRS = {
    "cargo_config": CRATES_VENDOR_ATTRS["cargo_config"],
    "cargo_lockfile": CRATES_VENDOR_ATTRS["cargo_lockfile"],
    "generate_binaries": CRATES_VENDOR_ATTRS["generate_binaries"],
    "generate_build_scripts": CRATES_VENDOR_ATTRS["generate_build_scripts"],
    "host_tools": attr.label(
        doc = "The `rust_host_tools` repository to use.",
        default = "@rust_host_tools",
    ),
    "isolated": attr.bool(
        doc = (
            "If true, `CARGO_HOME` will be overwritten to a directory within the generated repository in " +
            "order to prevent other uses of Cargo from impacting having any effect on the generated targets " +
            "produced by this rule. For users who either have multiple `crate_repository` definitions in a " +
            "WORKSPACE or rapidly re-pin dependencies, setting this to false may improve build times. This " +
            "variable is also controlled by `CARGO_BAZEL_ISOLATED` environment variable."
        ),
        default = True,
    ),
    "lockfile": attr.label(
        doc = (
            "The path to a file to use for reproducible renderings. " +
            "If set, this file must exist within the workspace (but can be empty) before this rule will work."
        ),
    ),
    "skip_cargo_lockfile_overwrite": attr.bool(
        doc = (
            "Whether to skip writing the cargo lockfile back after resolving. " +
            "You may want to set this if your dependency versions are maintained externally through a non-trivial set-up. " +
            "But you probably don't want to set this."
        ),
        default = False,
    ),
    "strip_internal_dependencies_from_cargo_lockfile": attr.bool(
        doc = (
            "Whether to strip internal dependencies from the cargo lockfile. " +
            "You may want to use this if you want to maintain a cargo lockfile for bazel only. " +
            "Bazel only requires external dependencies to be present in the lockfile. " +
            "By removing internal dependencies, the lockfile changes less frequently which reduces merge conflicts " +
            "in other lockfiles where the cargo lockfile's sha is stored."
        ),
        default = False,
    ),
    "supported_platform_triples": attr.string_list(
        doc = "A set of all platform triples to consider when generating dependencies.",
        default = SUPPORTED_PLATFORM_TRIPLES,
    ),
}

_from_cargo = tag_class(
    doc = "Generates a repo @crates from a Cargo.toml / Cargo.lock pair.",
    # Ordering is controlled for readability in generated docs.
    attrs = {
        "name": attr.string(
            doc = "The name of the repo to generate",
            default = "crates",
        ),
    } | {
        "manifests": CRATES_VENDOR_ATTRS["manifests"],
    } | _FROM_COMMON_ATTRS,
)

# This should be kept in sync with crate_universe/private/crate.bzl.
_ANNOTATION_ATTRS = {
    "crate": attr.string(
        doc = "The name of the crate the annotation is applied to",
        mandatory = True,
    ),
    "repositories": attr.string_list(
        doc = "A list of repository names specified from `crate.from_cargo(name=...)` that this annotation is applied to. Defaults to all repositories.",
        default = [],
    ),
    "version": attr.string(
        doc = "The versions of the crate the annotation is applied to. Defaults to all versions.",
        default = "*",
    ),
}

_ANNOTATION_NORMAL_ATTRS = {
    "additive_build_file": attr.label(
        doc = "A file containing extra contents to write to the bottom of generated BUILD files.",
    ),
    "additive_build_file_content": attr.string(
        doc = "Extra contents to write to the bottom of generated BUILD files.",
    ),
    "alias_rule": attr.string(
        doc = "Alias rule to use instead of `native.alias()`.  Overrides [render_config](#render_config)'s 'default_alias_rule'.",
    ),
    "build_script_data_glob": attr.string_list(
        doc = "A list of glob patterns to add to a crate's `cargo_build_script::data` attribute",
    ),
    "build_script_toolchains": attr.label_list(
        doc = "A list of labels to set on a crates's `cargo_build_script::toolchains` attribute.",
    ),
    "compile_data_glob": attr.string_list(
        doc = "A list of glob patterns to add to a crate's `rust_library::compile_data` attribute.",
    ),
    "compile_data_glob_excludes": attr.string_list(
        doc = "A list of glob patterns to be excllued from a crate's `rust_library::compile_data` attribute.",
    ),
    "data_glob": attr.string_list(
        doc = "A list of glob patterns to add to a crate's `rust_library::data` attribute.",
    ),
    "disable_pipelining": attr.bool(
        doc = "If True, disables pipelining for library targets for this crate.",
    ),
    "extra_aliased_targets": attr.string_dict(
        doc = "A list of targets to add to the generated aliases in the root crate_universe repository.",
    ),
    "gen_all_binaries": attr.bool(
        doc = "If true, generates `rust_binary` targets for all of the crates bins",
    ),
    "gen_binaries": attr.string_list(
        doc = "As a list, the subset of the crate's bins that should get `rust_binary` targets produced.",
    ),
    "gen_build_script": attr.string(
        doc = "An authoritative flag to determine whether or not to produce `cargo_build_script` targets for the current crate. Supported values are 'on', 'off', and 'auto'.",
        values = _OPT_BOOL_VALUES.keys(),
        default = "auto",
    ),
    "override_target_bin": attr.label(
        doc = "An optional alternate target to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
    ),
    "override_target_build_script": attr.label(
        doc = "An optional alternate target to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
    ),
    "override_target_lib": attr.label(
        doc = "An optional alternate target to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
    ),
    "override_target_proc_macro": attr.label(
        doc = "An optional alternate target to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
    ),
    "patch_args": attr.string_list(
        doc = "The `patch_args` attribute of a Bazel repository rule. See [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)",
    ),
    "patch_tool": attr.string(
        doc = "The `patch_tool` attribute of a Bazel repository rule. See [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)",
    ),
    "patches": attr.label_list(
        doc = "The `patches` attribute of a Bazel repository rule. See [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)",
    ),
    "shallow_since": attr.string(
        doc = "An optional timestamp used for crates originating from a git repository instead of a crate registry. This flag optimizes fetching the source code.",
    ),
}

_ANNOTATION_SELECT_ATTRS = {
    "build_script_compile_data": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::compile_data` attribute.",
    ),
    "build_script_data": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::data` attribute.",
    ),
    "build_script_deps": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::deps` attribute.",
    ),
    "build_script_env": attr.string_dict(
        doc = "Additional environment variables to set on a crate's `cargo_build_script::env` attribute.",
    ),
    "build_script_exec_properties": attr.string_dict(
        doc = "Execution properties to set on a crate's `cargo_build_script::exec_properties` attribute.",
    ),
    "build_script_link_deps": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::link_deps` attribute.",
    ),
    "build_script_proc_macro_deps": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::proc_macro_deps` attribute.",
    ),
    "build_script_rundir": attr.string(
        doc = "An override for the build script's rundir attribute.",
    ),
    "build_script_rustc_env": attr.string_dict(
        doc = "Additional environment variables to set on a crate's `cargo_build_script::env` attribute.",
    ),
    "build_script_tools": _relative_label_list(
        doc = "A list of labels to add to a crate's `cargo_build_script::tools` attribute.",
    ),
    "compile_data": _relative_label_list(
        doc = "A list of labels to add to a crate's `rust_library::compile_data` attribute.",
    ),
    "crate_features": attr.string_list(
        doc = "A list of strings to add to a crate's `rust_library::crate_features` attribute.",
    ),
    "data": _relative_label_list(
        doc = "A list of labels to add to a crate's `rust_library::data` attribute.",
    ),
    "deps": _relative_label_list(
        doc = "A list of labels to add to a crate's `rust_library::deps` attribute.",
    ),
    "proc_macro_deps": _relative_label_list(
        doc = "A list of labels to add to a crate's `rust_library::proc_macro_deps` attribute.",
    ),
    "rustc_env": attr.string_dict(
        doc = "Additional variables to set on a crate's `rust_library::rustc_env` attribute.",
    ),
    "rustc_env_files": _relative_label_list(
        doc = "A list of labels to set on a crate's `rust_library::rustc_env_files` attribute.",
    ),
    "rustc_flags": attr.string_list(
        doc = "A list of strings to set on a crate's `rust_library::rustc_flags` attribute.",
    ),
}

_annotation = tag_class(
    doc = "A collection of extra attributes and settings for a particular crate.",
    attrs = _ANNOTATION_ATTRS | _ANNOTATION_NORMAL_ATTRS | _ANNOTATION_SELECT_ATTRS,
)

_from_specs = tag_class(
    doc = "Generates a repo @crates from the defined `spec` tags.",
    attrs = {
        "name": attr.string(
            doc = "The name of the repo to generate.",
            default = "crates",
        ),
    } | _FROM_COMMON_ATTRS,
)

# This should be kept in sync with crate_universe/private/crate.bzl.
_spec = tag_class(
    doc = "A constructor for a crate dependency.",
    attrs = {
        "artifact": attr.string(
            doc = "Set to 'bin' to pull in a binary crate as an artifact dependency. Requires a nightly Cargo.",
        ),
        "branch": attr.string(
            doc = "The git branch of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.",
        ),
        "default_features": attr.bool(
            doc = "Maps to the `default-features` flag.",
            default = True,
        ),
        "features": attr.string_list(
            doc = "A list of features to use for the crate.",
        ),
        "git": attr.string(
            doc = "The Git url to use for the crate. Cannot be used with `version` or `path`.",
        ),
        "lib": attr.bool(
            doc = "If using `artifact = 'bin'`, additionally setting `lib = True` declares a dependency on both the package's library and binary, as opposed to just the binary.",
        ),
        "package": attr.string(
            doc = "The explicit name of the package.",
            mandatory = True,
        ),
        "path": attr.string(
            doc = "The local path of the remote crate. Cannot be used with `version` or `git`.",
        ),
        "repositories": attr.string_list(
            doc = "A list of repository names specified from `crate.from_cargo(name=...)` that this spec is applied to. Defaults to all repositories.",
            default = [],
        ),
        "rev": attr.string(
            doc = "The git revision of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified.",
        ),
        "tag": attr.string(
            doc = "The git tag of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.",
        ),
        "version": attr.string(
            doc = "The exact version of the crate. Cannot be used with `git` or `path`.",
        ),
    },
)

_splicing_config = tag_class(
    doc = "Various settings used to configure Cargo manifest splicing behavior.",
    attrs = {
        "repositories": attr.string_list(
            doc = "A list of repository names specified from `crate.from_cargo(name=...)` that this annotation is applied to. Defaults to all repositories.",
            default = [],
        ),
    } | {
        "resolver_version": attr.string(
            doc = "The [resolver version](https://doc.rust-lang.org/cargo/reference/resolver.html#resolver-versions) to use in generated Cargo manifests. This flag is **only** used when splicing a manifest from direct package definitions. See `crates_repository::packages`",
            default = "2",
        ),
    },
)

_render_config = tag_class(
    doc = """\
Various settings used to configure rendered outputs.

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
""",
    attrs = {
        "repositories": attr.string_list(
            doc = "A list of repository names specified from `crate.from_cargo(name=...)` that this annotation is applied to. Defaults to all repositories.",
            default = [],
        ),
    } | {
        "build_file_template": attr.string(
            doc = "The base template to use for BUILD file names. The available format keys are [`{name}`, {version}`].",
            default = "//:BUILD.{name}-{version}.bazel",
        ),
        "crate_alias_template": attr.string(
            doc = "The base template to use for crate labels. The available format keys are [`{repository}`, `{name}`, `{version}`, `{target}`].",
            default = "@{repository}//:{name}-{version}-{target}",
        ),
        "crate_label_template": attr.string(
            doc = "The base template to use for crate labels. The available format keys are [`{repository}`, `{name}`, `{version}`, `{target}`].",
            default = "@{repository}__{name}-{version}//:{target}",
        ),
        "crate_repository_template": attr.string(
            doc = "The base template to use for Crate label repository names. The available format keys are [`{repository}`, `{name}`, `{version}`].",
            default = "{repository}__{name}-{version}",
        ),
        "crates_module_template": attr.string(
            doc = "The pattern to use for the `defs.bzl` and `BUILD.bazel` file names used for the crates module. The available format keys are [`{file}`].",
            default = "//:{file}",
        ),
        "default_alias_rule_bzl": attr.label(
            doc = "Alias rule to use when generating aliases for all crates.  Acceptable values are 'alias', 'dbg'/'fastbuild'/'opt' (transitions each crate's `compilation_mode`)  or a string representing a rule in the form '<label to .bzl>:<rule>' that takes a single label parameter 'actual'. See '@crate_index//:alias_rules.bzl' for an example.",
        ),
        "default_alias_rule_name": attr.string(
            doc = "Alias rule to use when generating aliases for all crates.  Acceptable values are 'alias', 'dbg'/'fastbuild'/'opt' (transitions each crate's `compilation_mode`)  or a string representing a rule in the form '<label to .bzl>:<rule>' that takes a single label parameter 'actual'. See '@crate_index//:alias_rules.bzl' for an example.",
            default = "alias",
        ),
        "default_package_name": attr.string(
            doc = "The default package name to use in the rendered macros. This affects the auto package detection of things like `all_crate_deps`.",
            default = "",
        ),
        "generate_cargo_toml_env_vars": attr.bool(
            doc = "Whether to generate cargo_toml_env_vars targets.",
            default = True,
        ),
        "generate_rules_license_metadata": attr.bool(
            doc = "Whether to generate rules license metadata.",
            default = False,
        ),
        "generate_target_compatible_with": attr.bool(
            doc = "Whether to generate `target_compatible_with` annotations on the generated BUILD files.  This catches a `target_triple` being targeted that isn't declared in `supported_platform_triples`.",
            default = True,
        ),
        "platforms_template": attr.string(
            doc = "The base template to use for platform names. See [platforms documentation](https://docs.bazel.build/versions/main/platforms.html). The available format keys are [`{triple}`].",
            default = "@rules_rust//rust/platform:{triple}",
        ),
        "regen_command": attr.string(
            doc = "An optional command to demonstrate how generated files should be regenerated.",
            default = "",
        ),
        "vendor_mode": attr.string(
            doc = "An optional configuration for rendering content to be rendered into repositories.",
            default = "",
        ),
    },
)

_annotation_select = tag_class(
    doc = "A constructor for a crate dependency with selectable attributes.",
    attrs = _ANNOTATION_ATTRS | {
        "triples": attr.string_list(
            doc = "A list of triples to apply the annotation to.",
            mandatory = True,
        ),
    } | _ANNOTATION_SELECT_ATTRS,
)

crate = module_extension(
    doc = """\
Crate universe module extensions.

Environment Variables:

| variable | usage |
| --- | --- |
| `CARGO_BAZEL_GENERATOR_SHA256` | The sha256 checksum of the file located at `CARGO_BAZEL_GENERATOR_URL` |
| `CARGO_BAZEL_GENERATOR_URL` | The URL of a cargo-bazel binary. This variable takes precedence over attributes and can use `file://` for local paths |
| `CARGO_BAZEL_ISOLATED` | An authoritative flag as to whether or not the `CARGO_HOME` environment variable should be isolated from the host configuration |
| `CARGO_BAZEL_REPIN` | An indicator that the dependencies represented by the rule should be regenerated. `REPIN` may also be used. See [Repinning / Updating Dependencies](crate_universe_workspace.html#repinning--updating-dependencies) for more details. |
| `CARGO_BAZEL_REPIN_ONLY` | A comma-delimited allowlist for rules to execute repinning. Can be useful if multiple instances of the repository rule are used in a Bazel workspace, but repinning should be limited to one of them. |
| `CARGO_BAZEL_TIMEOUT` | An integer value to override the default timeout setting when running the cargo-bazel binary. This value must be in seconds. |

""",
    implementation = _crate_impl,
    tag_classes = {
        "annotation": _annotation,
        "annotation_select": _annotation_select,
        "from_cargo": _from_cargo,
        "from_specs": _from_specs,
        "render_config": _render_config,
        "spec": _spec,
        "splicing_config": _splicing_config,
    },
    environ = CRATES_REPOSITORY_ENVIRON,
)
