# Bazel yq rule

From the documentation at https://mikefarah.gitbook.io/yq:

> yq is a a lightweight and portable command-line YAML processor.
> yq uses jq-like syntax but works with yaml files as well as json.

## Usage examples

```starlark
load("@yq.bzl", "yq")
```

```starlark
# Remove fields
yq(
    name = "safe-config",
    srcs = ["config.yaml"],
    expression = "del(.credentials)",
)
```

```starlark
# Merge two yaml documents
yq(
    name = "ab",
    srcs = [
        "a.yaml",
        "b.yaml",
    ],
    expression = ". as $item ireduce ({}; . * $item )",
)
```

```starlark
# Split a yaml file into several files
yq(
    name = "split",
    srcs = ["multidoc.yaml"],
    outs = [
        "first.yml",
        "second.yml",
    ],
    args = [
        "-s '.a'",  # Split expression
        "--no-doc", # Exclude document separator --
    ],
)
```

```starlark
# Convert a yaml file to json
yq(
    name = "convert-to-json",
    srcs = ["foo.yaml"],
    args = ["-o=json"],
    outs = ["foo.json"],
)
```

```starlark
# Convert a json file to yaml
yq(
    name = "convert-to-yaml",
    srcs = ["bar.json"],
    args = ["-P"],
    outs = ["bar.yaml"],
)
```

```starlark
# Call yq in a genrule
genrule(
    name = "generate",
    srcs = ["farm.yaml"],
    outs = ["genrule_output.yaml"],
    cmd = "$(YQ_BIN) '.moo = \"cow\"' $(location farm.yaml) > $@",
    toolchains = ["@yq_toolchains//:resolved_toolchain"],
)
```

```starlark
# With --stamp, causes properties to be replaced by version control info.
yq(
    name = "stamped",
    srcs = ["package.yaml"],
    expression = "|".join([
        "load(strenv(STAMP)) as $stamp",
        # Provide a default using the "alternative operator" in case $stamp is empty dict.
        ".version = ($stamp.BUILD_EMBED_LABEL // "<unstamped>")",
    ]),
)
```
