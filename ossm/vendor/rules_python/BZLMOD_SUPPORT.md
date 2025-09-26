# Bzlmod support

## `rules_python` `bzlmod` support

- Status: GA
- Full Feature Parity: No
    - `rules_python`: Yes
    - `rules_python_gazelle_plugin`: No (see below).

In general `bzlmod` has more features than `WORKSPACE` and users are encouraged to migrate.

## Configuration

The releases page will give you the latest version number, and a basic example.  The release page is located [here](/bazel-contrib/rules_python/releases).

## What is bzlmod?

> Bazel supports external dependencies, source files (both text and binary) used in your build that are not from your workspace. For example, they could be a ruleset hosted in a GitHub repo, a Maven artifact, or a directory on your local machine outside your current workspace.
>
> As of Bazel 6.0, there are two ways to manage external dependencies with Bazel: the traditional, repository-focused WORKSPACE system, and the newer module-focused MODULE.bazel system (codenamed Bzlmod, and enabled with the flag `--enable_bzlmod`). The two systems can be used together, but Bzlmod is replacing the WORKSPACE system in future Bazel releases.
> -- <cite>https://bazel.build/external/overview</cite>

## Examples

We have two examples that demonstrate how to configure `bzlmod`.

The first example is in [examples/bzlmod](examples/bzlmod), and it demonstrates basic bzlmod configuration.
A user does not use `local_path_override` stanza and would define the version in the `bazel_dep` line.

A second example, in [examples/bzlmod_build_file_generation](examples/bzlmod_build_file_generation) demonstrates the use of `bzlmod` to configure `gazelle` support for `rules_python`.

## Differences in behavior from WORKSPACE

### Default toolchain is not the local system Python

Under bzlmod, the default toolchain is no longer based on the locally installed
system Python. Instead, a recent Python version using the pre-built,
standalone runtimes are used.

If you need the local system Python to be your toolchain, then it's suggested
that you setup and configure your own toolchain and register it. Note that using
the local system's Python is not advised because will vary between users and
platforms.

If you want to use the same toolchain as what WORKSPACE used, then manually
register the builtin Bazel Python toolchain by doing
`register_toolchains("@bazel_tools//tools/python:autodetecting_toolchain")`.

Note that using this builtin Bazel toolchain is deprecated and unsupported.
See the {obj}`runtime_env_toolchains` docs for a replacement that is marginally
better supported.
**IMPORTANT: this should only be done in a root module, and may interfere with
the toolchains rules_python registers**.

NOTE: Regardless of your toolchain, due to
[#691](https://github.com/bazel-contrib/rules_python/issues/691), `rules_python`
still relies on a local Python being available to bootstrap the program before
handing over execution to the toolchain Python.

To override this behaviour see {obj}`--bootstrap_impl=script`, which switches
to `bash`-based bootstrap on UNIX systems.

### Better PyPI package downloading on bzlmod

On `bzlmod` users have the option to use the `bazel_downloader` to download packages
and work correctly when `host` platform is not the same as the `target` platform. This
provides faster package download times and integration with the credentials helper.

### Extra targets in `whl_library` repos

Due to how `bzlmod` is designed and the visibility rules that it enforces, it is best to use
the targets in the `whl` repos as they do not rely on using the `annotations` API to
add extra targets to so-called `spoke` repos. For alternatives that should cover most of the
existing usecases please see:
* {bzl:obj}`py_console_script_binary` to create `entry_point` targets.
* {bzl:obj}`whl_filegroup` to extract filegroups from the `whl` targets (e.g. `@pip//numpy:whl`)
* {bzl:obj}`pip.override` to patch the downloaded `whl` files. Using that you
  can change the `METADATA` of the `whl` file that will influence how
  `rules_python` code generation behaves.
