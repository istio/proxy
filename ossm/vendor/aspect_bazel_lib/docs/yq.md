<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Wrapper rule around the `yq` tool

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
    cmd = "$(YQ_BIN) '.moo = "cow"' $(location farm.yaml) > $@",
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

<a id="yq"></a>

## yq

<pre>
yq(<a href="#yq-name">name</a>, <a href="#yq-srcs">srcs</a>, <a href="#yq-expression">expression</a>, <a href="#yq-args">args</a>, <a href="#yq-outs">outs</a>, <a href="#yq-kwargs">kwargs</a>)
</pre>

Invoke yq with an expression on a set of input files.

yq is capable of parsing and outputting to other formats. See their [docs](https://mikefarah.gitbook.io/yq) for more examples.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="yq-name"></a>name |  Name of the rule   |  none |
| <a id="yq-srcs"></a>srcs |  List of input file labels   |  none |
| <a id="yq-expression"></a>expression |  yq expression (https://mikefarah.gitbook.io/yq/commands/evaluate).<br><br>Defaults to the identity expression ".". Subject to stamp variable replacements, see [Stamping](./stamping.md). When stamping is enabled, an environment variable named "STAMP" will be available in the expression.<br><br>Be careful to write the filter so that it handles unstamped builds, as in the example above.   |  `"."` |
| <a id="yq-args"></a>args |  Additional args to pass to yq.<br><br>Note that you do not need to pass _eval_ or _eval-all_ as this is handled automatically based on the number `srcs`. Passing the output format or the parse format is optional as these can be guessed based on the file extensions in `srcs` and `outs`.   |  `[]` |
| <a id="yq-outs"></a>outs |  Name of the output files.<br><br>Defaults to a single output with the name plus a ".yaml" extension, or the extension corresponding to a passed output argument (e.g., "-o=json"). For split operations you must declare all outputs as the name of the output files depends on the expression.   |  `None` |
| <a id="yq-kwargs"></a>kwargs |  Other common named parameters such as `tags` or `visibility`   |  none |


