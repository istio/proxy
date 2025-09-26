"""Wrapper rule around the `yq` tool

From the documentation at https://mikefarah.gitbook.io/yq:

> yq is a a lightweight and portable command-line YAML processor.
> yq uses jq like syntax but works with yaml files as well as json.

## Usage examples

```starlark
load("@aspect_bazel_lib//lib:yq.bzl", "yq")
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
"""

load("//lib/private:yq.bzl", _is_split_operation = "is_split_operation", _yq_lib = "yq_lib")

_yq_rule = rule(
    attrs = _yq_lib.attrs,
    implementation = _yq_lib.implementation,
    toolchains = ["@aspect_bazel_lib//lib:yq_toolchain_type"],
)

def yq(name, srcs, expression = ".", args = [], outs = None, **kwargs):
    """Invoke yq with an expression on a set of input files.

    yq is capable of parsing and outputting to other formats. See their [docs](https://mikefarah.gitbook.io/yq) for more examples.

    Args:
        name: Name of the rule
        srcs: List of input file labels
        expression: yq expression (https://mikefarah.gitbook.io/yq/commands/evaluate).

            Defaults to the identity
            expression ".". Subject to stamp variable replacements, see [Stamping](./stamping.md).
            When stamping is enabled, an environment variable named "STAMP" will be available in the expression.

            Be careful to write the filter so that it handles unstamped builds, as in the example above.

        args: Additional args to pass to yq.

            Note that you do not need to pass _eval_ or _eval-all_ as this
            is handled automatically based on the number `srcs`. Passing the output format or the parse format
            is optional as these can be guessed based on the file extensions in `srcs` and `outs`.

        outs: Name of the output files.

            Defaults to a single output with the name plus a ".yaml" extension, or
            the extension corresponding to a passed output argument (e.g., "-o=json"). For split operations you
            must declare all outputs as the name of the output files depends on the expression.

        **kwargs: Other common named parameters such as `tags` or `visibility`
    """
    args = args[:]

    if not _is_split_operation(args):
        # For split operations we can't predeclare outs because the name of the resulting files
        # depends on the expression. For non-split operations, set a default output file name
        # based on the name and the output format passed, defaulting to yaml.
        if not outs:
            outs = [name + ".yaml"]
            if "-o=json" in args or "--outputformat=json" in args:
                outs = [name + ".json"]
            if "-o=xml" in args or "--outputformat=xml" in args:
                outs = [name + ".xml"]
            elif "-o=props" in args or "--outputformat=props" in args:
                outs = [name + ".properties"]
            elif "-o=c" in args or "--outputformat=csv" in args:
                outs = [name + ".csv"]
            elif "-o=t" in args or "--outputformat=tsv" in args:
                outs = [name + ".tsv"]

        elif outs and len(outs) == 1:
            # If an output file with an extension was provided, try to set the corresponding output
            # argument if it wasn't already passed.
            if outs[0].endswith(".json") and "-o=json" not in args and "--outputformat=json" not in args:
                args.append("-o=json")
            elif outs[0].endswith(".xml") and "-o=xml" not in args and "--outputformat=xml" not in args:
                args.append("-o=xml")
            elif outs[0].endswith(".properties") and "-o=props" not in args and "--outputformat=props" not in args:
                args.append("-o=props")
            elif outs[0].endswith(".csv") and "-o=c" not in args and "--outputformat=csv" not in args:
                args.append("-o=c")
            elif outs[0].endswith(".tsv") and "-o=t" not in args and "--outputformat=tsv" not in args:
                args.append("-o=t")

    # If the input files are json or xml, set the parse flag if it isn't already set
    if len(srcs) > 0:
        if srcs[0].endswith(".json") and "-P" not in args:
            args.append("-P")
        elif srcs[0].endswith(".xml") and "-p=xml" not in args:
            args.append("-p=xml")

    _yq_rule(
        name = name,
        srcs = srcs,
        expression = expression,
        args = args,
        outs = outs,
        **kwargs
    )
