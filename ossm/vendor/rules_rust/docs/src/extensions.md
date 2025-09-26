# Extensions

Bazel rules for interfacing with other rules and integrations with popular 3rd party tools.

## Setup

The extension rules are released with each release of `rules_rust` (core) which can be found on [the GitHub Releases page](https://github.com/bazelbuild/rules_rust/releases). We recommend using the latest release from that page.

### Bzlmod

Note that rules_rust bzlmod support is still a work in progress. Most features should work, but bugs are more likely. This is not a desired end-state - please report (or better yet, help fix!) bugs you run into.

To use `rules_rust` extensions in a project using bzlmod, add the following to your `MODULE.bazel` file:

```python
bazel_dep(name = "rules_rust_{EXTENSION}", version = "{VERSION}")
```

Don't forget to substitute in your desired release's version number and `{EXTENSION}` with the
name of the desired extension. E.g.:

```python
bazel_dep(name = "rules_rust_bindgen", version = "0.55.0")
```

### WORKSPACE

To use `rules_rust` extensions in a project using a WORKSPACE file, add the following to your `WORKSPACE` file to add the external repositories for the Rust toolchain:

```python
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# To find additional information on this release or newer ones visit:
# https://github.com/bazelbuild/rules_rust/releases
http_archive(
    name = "rules_rust_{EXTENSION}",
    # See releases page
)

# Refer to the documentation of the desired rules for how to load other necessary dependencies.
```

Don't forget to substitute in your desired release's version number, integrity hash, and `{EXTENSION}` with the name of the desired extension.
