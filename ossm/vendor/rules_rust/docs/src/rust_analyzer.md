# Rust Analyzer

For [non-Cargo projects](https://rust-analyzer.github.io/manual.html#non-cargo-based-projects),
[rust-analyzer](https://rust-analyzer.github.io/) depends on either a `rust-project.json` file
at the root of the project that describes its structure or on build system specific
[project auto-discovery](https://rust-analyzer.github.io/manual.html#rust-analyzer.workspace.discoverConfig).
The `rust_analyzer` rules facilitate both approaches.

## rust-project.json approach

### Setup

#### Bzlmod

First, ensure `rules_rust` is setup in your workspace:

```python
# MODULE.bazel

# See releases page for available versions:
# https://github.com/bazelbuild/rules_rust/releases
bazel_dep(name = "rules_rust", version = "{SEE_RELEASES}")
```

Bazel will create the target `@rules_rust//tools/rust_analyzer:gen_rust_project`, which you can build
with

```
bazel run @rules_rust//tools/rust_analyzer:gen_rust_project
```

whenever dependencies change to regenerate the `rust-project.json` file. It
should be added to `.gitignore` because it is effectively a build artifact.
Once the `rust-project.json` has been generated in the project root,
rust-analyzer can pick it up upon restart.

#### WORKSPACE

Alternatively, you can use the legacy WORKSPACE approach. As with Bzlmod, ensure `rules_rust` is
setup in your workspace.

Moreover, when loading the dependencies for the tool, you should call the function `rust_analyzer_dependencies()`:

```python
load("@rules_rust//tools/rust_analyzer:deps.bzl", "rust_analyzer_dependencies")

rust_analyzer_dependencies()
```

Again, you can now run `bazel run @rules_rust//tools/rust_analyzer:gen_rust_project`
whenever dependencies change to regenerate the `rust-project.json` file.

For users who do not use `rust_register_toolchains` to register toolchains, the following can be added
to their WORKSPACE to register a `rust_analyzer_toolchain`. Please make sure the Rust version used in
this toolchain matches the version used by the currently registered toolchain or the sources/documentation
will not match what's being compiled with and can lead to confusing results.

```python
load("@rules_rust//rust:repositories.bzl", "rust_analyzer_toolchain_repository")

register_toolchains(rust_analyzer_toolchain_repository(
    name = "rust_analyzer_toolchain",
    # This should match the currently registered toolchain.
    version = "1.63.0",
))
```

#### VSCode

To set this up using [VSCode](https://code.visualstudio.com/), users should first install the
[rust_analyzer plugin](https://marketplace.visualstudio.com/items?itemName=matklad.rust-analyzer).
With that in place, the following task can be added to the `.vscode/tasks.json` file of the workspace
to ensure a `rust-project.json` file is created and up to date when the editor is opened.

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Generate rust-project.json",
            "command": "bazel",
            "args": [
                "run",
                "@rules_rust//tools/rust_analyzer:gen_rust_project"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "group": "build",
            "problemMatcher": [],
            "presentation": {
                "reveal": "never",
                "panel": "dedicated"
            },
            "runOptions": {
                "runOn": "folderOpen"
            }
        }
    ]
}
```

#### Alternative vscode option (prototype)

Add the following to your bazelrc:

```
build --@rules_rust//rust/settings:rustc_output_diagnostics=true --output_groups=+rust_lib_rustc_output,+rust_metadata_rustc_output
```

Then you can use a prototype [rust-analyzer plugin](https://marketplace.visualstudio.com/items?itemName=MattStark.bazel-rust-analyzer) that automatically collects the outputs whenever you recompile.

## Project auto-discovery

### Setup

Auto-discovery makes `rust-analyzer` behave in a Bazel project in a similar fashion to how it does
in a Cargo project. This is achieved by generating a structure similar to what `rust-project.json`
contains but, instead of writing that to a file, the data gets piped to `rust-analyzer` directly
through `stdout`. To use auto-discovery the `rust-analyzer` IDE settings must be configured similar to:

```json
"rust-analyzer": {
    "workspace": {
        "discoverConfig": {
            "command": ["discover_bazel_rust_project.sh"],
            "progressLabel": "rust_analyzer",
            "filesToWatch": ["BUILD", "BUILD.bazel", "MODULE.bazel"]
        }
    }
}
```

The shell script passed to `discoverConfig.command` is typically meant to wrap the bazel rule invocation,
primarily for muting `stderr` (because `rust-analyzer` will consider that an error has occurred if anything
is passed through `stderr`) and, additionally, for specifying rule arguments. E.g:

```shell
#!/usr/bin/bash

bazel \
    run \
    @rules_rust//tools/rust_analyzer:discover_bazel_rust_project -- \
    --bazel_startup_option=--output_base=~/ide_bazel \
    --bazel_arg=--watchfs \
    ${1:+"$1"} 2>/dev/null
```

The script above also handles an optional CLI argument which gets passed when workspace splitting is
enabled. The script path should be either absolute or relative to the project root.

### Workspace splitting

The above configuration treats the entire project as a single workspace. However, large codebases might be
too much to handle for `rust-analyzer` all at once. This can be addressed by splitting the codebase in
multiple workspaces by extending the `discoverConfig.command` setting:

```json
"rust-analyzer": {
    "workspace": {
        "discoverConfig": {
            "command": ["discover_bazel_rust_project.sh", "{arg}"],
            "progressLabel": "rust_analyzer",
            "filesToWatch": ["BUILD", "BUILD.bazel", "MODULE.bazel"]
        }
    }
}
```

`{arg}` acts as a placeholder that `rust-analyzer` replaces with the path of the source / build file
that gets opened.

The root of the workspace will, in this configuration, be the package the crate currently being worked on
belongs to. This means that only that package and its dependencies get built and indexed by `rust-analyzer`,
thus allowing for a smaller footprint.

`rust-analyzer` will switch workspaces whenever an out-of-tree file gets opened, essentially indexing that
crate and its dependencies separately. A caveat of this is that _dependents_ of the crate currently being
worked on are not indexed and won't be tracked by `rust-analyzer`.
