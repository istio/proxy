# Bazel helpers library

Base Starlark libraries and basic Bazel rules which are useful for constructing rulesets and BUILD files.

This module depends on [bazel-skylib](https://github.com/bazelbuild/bazel-skylib).
In theory all these utilities could be upstreamed to bazel-skylib, but the declared scope of that project
is narrow and it's very difficult to get anyone's attention to review PRs there.

## Installation

Installation instructions are included on each release:
<https://github.com/bazel-contrib/bazel-lib/releases>

To use a commit rather than a release, you can point at any SHA of the repo.
However, this adds more "dev dependencies", as you'll have to build our helper programs (such as `copy_to_directory`, `expand_template`) from their Go sources rather than
download pre-built binaries.

For example to use commit `abc123` in `MODULE.bazel`:

```
# Automatically picks up new Go dev dependencies
git_override(
    module_name = "aspect_bazel_lib",
    commit = "abc123",
    remote = "git@github.com:aspect-build/bazel-lib.git",
)
```

Or in `WORKSPACE`:

1. Replace `url = "https://github.com/bazel-contrib/bazel-lib/releases/download/v0.1.0/bazel-lib-v0.1.0.tar.gz"`
   with a GitHub-provided source archive like
   `url = "https://github.com/bazel-contrib/bazel-lib/archive/abc123.tar.gz"`
1. Replace `strip_prefix = "bazel-lib-0.1.0"` with `strip_prefix = "bazel-lib-abc123"`
1. Update the `sha256`. The easiest way to do this is to comment out the line, then Bazel will
   print a message with the correct value.
1. `load("@aspect_bazel_lib//:deps.bzl", "go_dependencies")` and then call `go_dependencies()`

> Note that GitHub source archives don't have a strong guarantee on the sha256 stability, see
> <https://github.blog/2023-02-21-update-on-the-future-stability-of-source-code-archives-and-hashes>

# Public API

## Packaging

- [tar](docs/tar.md) Run BSD `tar` to produce archives.

## Copying files

- [copy_directory](docs/copy_directory.md) Copies directories to another package.
- [copy_file](docs/copy_file.md) Copies files to another package.
- [copy_to_bin](docs/copy_to_bin.md) Copies a source file to output tree at the same workspace-relative path.
- [copy_to_directory](docs/copy_to_directory.md) Copies and arranges files and directories into a new directory.
- [write_source_files](docs/write_source_files.md) Write to one or more files or folders in the source tree. Stamp out tests that ensure the sources exist and are up to date.

## Transforming files

- [jq](docs/jq.md) A toolchain and custom rule for running [jq](https://stedolan.github.io/jq/), a tool that is "like sed for json".
- [yq](docs/yq.md) A toolchain and custom rule for running [yq](https://github.com/mikefarah/yq), a "YAML, JSON and XML processor".

## Manipulating paths

- [directory_path](docs/directory_path.md) Provide a label to reference some path within a directory, via DirectoryPathInfo.
- [output_files](docs/output_files.md) Forwards a subset of the files (via the DefaultInfo provider) from a given target's DefaultInfo or OutputGroupInfo.

## Writing rules

- [expand_make_vars](docs/expand_make_vars.md) Perform make variable and location substitutions in strings..
- [expand_template](docs/expand_template.md) Substitute templates with make variables, location resolves, stamp variables, and arbitrary strings.
- [paths](docs/paths.md) Useful path resolution methods.
- [transitions](docs/transitions.md) Transition sources to a provided platform.
- [lists](docs/lists.md) Functional-style helpers for working with list data structures.
- [utils](docs/utils.md) Various utils for labels and globs.
- [params_file](docs/params_file.md) Generate encoded params file from a list of arguments.
- [repo_utils](docs/repo_utils.md) Useful methods for repository rule implementations.
- [run_binary](docs/run_binary.md) Like skylib's run_binary but adds directory output support.
- [stamping](docs/stamping.md) Support version stamping in custom rules.
- [base64](docs/base64.md) Starlark Base64 encoder & decoder.

## Testing

- [bats_test](docs/bats.md): A test runner for the [Bash Automated Testing System](https://github.com/bats-core/bats-core).

## Generating documentation

- [docs](docs/docs.md) Rules for generating docs and stamping tests to ensure they are up to date.
