# Glint Bazel Integration

This directory contains Bazel repository rules for downloading and using the `glint` binary.

## Usage

To use glint in your Bazel workspace, add the following to your `WORKSPACE` file:

```starlark
load("@envoy_toolshed//bazel/format/glint:glint_repository.bzl", "glint_repository")

glint_repository(
    bins_release_version = "0.1.21",  # Use the version you want
)
```

Then you can reference the glint binary in your BUILD files:

```starlark
# Use as a tool in a genrule (shorthand)
genrule(
    name = "check_files",
    srcs = ["file.txt"],
    outs = ["checked.txt"],
    tools = ["@glint"],
    cmd = "$(location @glint) check $(location file.txt) > $@",
)

# Or use the full form
genrule(
    name = "check_files_explicit",
    srcs = ["file.txt"],
    outs = ["checked2.txt"],
    tools = ["@glint//:glint"],
    cmd = "$(location @glint//:glint) check $(location file.txt) > $@",
)

# Or use the glint_bin filegroup
filegroup(
    name = "glint_tool",
    srcs = ["@glint//:glint_bin"],
)
```

## Platform Support

The glint binary is automatically downloaded for your platform:
- Linux x86_64 (amd64)
- Linux aarch64 (arm64)

The binaries are downloaded from the GitHub releases at:
`https://github.com/envoyproxy/toolshed/releases/download/bins-v{version}/glint-{glint_version}-{arch}`
