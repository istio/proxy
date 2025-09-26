<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Analyzer

* [rust_analyzer_aspect](#rust_analyzer_aspect)
* [rust_analyzer_toolchain](#rust_analyzer_toolchain)


## Overview

For [non-Cargo projects](https://rust-analyzer.github.io/manual.html#non-cargo-based-projects),
[rust-analyzer](https://rust-analyzer.github.io/) depends on a `rust-project.json` file at the
root of the project that describes its structure. The `rust_analyzer` rule facilitates generating
such a file.

### Setup

First, ensure `rules_rust` is setup in your workspace. By default, `rust_register_toolchains` will
ensure a [rust_analyzer_toolchain](#rust_analyzer_toolchain) is registered within the WORKSPACE.

Next, load the dependencies for the `rust-project.json` generator tool:

```python
load("@rules_rust//tools/rust_analyzer:deps.bzl", "rust_analyzer_dependencies")

rust_analyzer_dependencies()
```

Finally, run `bazel run @rules_rust//tools/rust_analyzer:gen_rust_project`
whenever dependencies change to regenerate the `rust-project.json` file. It
should be added to `.gitignore` because it is effectively a build artifact.
Once the `rust-project.json` has been generated in the project root,
rust-analyzer can pick it up upon restart.

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
                "//tools/rust_analyzer:gen_rust_project"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "group": "build",
            "problemMatcher": [],
            "presentation": {
                "reveal": "never",
                "panel": "dedicated",
            },
            "runOptions": {
                "runOn": "folderOpen"
            }
        },
    ]
}
```

#### Alternative vscode option (prototype)

Add the following to your bazelrc:
```
build --@rules_rust//rust/settings:rustc_output_diagnostics=true --output_groups=+rust_lib_rustc_output,+rust_metadata_rustc_output
```

Then you can use a prototype [rust-analyzer plugin](https://marketplace.visualstudio.com/items?itemName=MattStark.bazel-rust-analyzer) that automatically collects the outputs whenever you recompile.



<a id="rust_analyzer_toolchain"></a>

## rust_analyzer_toolchain

<pre>
rust_analyzer_toolchain(<a href="#rust_analyzer_toolchain-name">name</a>, <a href="#rust_analyzer_toolchain-proc_macro_srv">proc_macro_srv</a>, <a href="#rust_analyzer_toolchain-rustc">rustc</a>, <a href="#rust_analyzer_toolchain-rustc_srcs">rustc_srcs</a>)
</pre>

A toolchain for [rust-analyzer](https://rust-analyzer.github.io/).

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_analyzer_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_analyzer_toolchain-proc_macro_srv"></a>proc_macro_srv |  The path to a `rust_analyzer_proc_macro_srv` binary.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_analyzer_toolchain-rustc"></a>rustc |  The path to a `rustc` binary.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_analyzer_toolchain-rustc_srcs"></a>rustc_srcs |  The source code of rustc.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="rust_analyzer_aspect"></a>

## rust_analyzer_aspect

<pre>
rust_analyzer_aspect(<a href="#rust_analyzer_aspect-name">name</a>)
</pre>

Annotates rust rules with RustAnalyzerInfo later used to build a rust-project.json

**ASPECT ATTRIBUTES**


| Name | Type |
| :------------- | :------------- |
| deps| String |
| proc_macro_deps| String |
| crate| String |
| actual| String |
| proto| String |


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_analyzer_aspect-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


