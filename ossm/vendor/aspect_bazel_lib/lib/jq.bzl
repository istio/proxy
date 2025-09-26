"""Wrapper rule around the popular `jq` utility.

For jq documentation, see https://stedolan.github.io/jq/.

## Usage examples

```starlark
load("@aspect_bazel_lib//lib:jq.bzl", "jq")
```

Create a new file `bazel-out/.../no_srcs.json` containing some JSON data:
```starlark
jq(
    name = "no_srcs",
    srcs = [],
    filter = ".name = \"Alice\"",
)
```

Remove a field from `package.json`:

> The output path `bazel-out/.../package.json` matches the path of the source file,
> which means you must refer to the label `:no_dev_deps` to reference the output,
> since Bazel doesn't provide a label for an output file that collides with an input file.

```starlark
jq(
    name = "no_dev_deps",
    srcs = ["package.json"],
    out = "package.json",
    filter = "del(.devDependencies)",
)
```

Merge data from `bar.json` on top of `foo.json`, producing `foobar.json`:
```starlark
jq(
    name = "merged",
    srcs = ["foo.json", "bar.json"],
    filter = ".[0] * .[1]",
    args = ["--slurp"],
    out = "foobar.json",
)
```

Long filters can be split over several lines with comments:
```starlark
jq(
    name = "complex",
    srcs = ["a.json", "b.json"],
    filter = \"\"\"
        .[0] as $a
        # Take select fields from b.json
        | (.[1] | {foo, bar, tags}) as $b
        # Merge b onto a
        | ($a * $b)
        # Combine 'tags' array from both
        | .tags = ($a.tags + $b.tags)
        # Add new field
        + {\\\"aspect_is_cool\\\": true}
    \"\"\",
    args = ["--slurp"],
)
```

Load filter from a file `filter.jq`, making it easier to edit complex filters:
```starlark
jq(
    name = "merged",
    srcs = ["foo.json", "bar.json"],
    filter_file = "filter.jq",
    args = ["--slurp"],
    out = "foobar.json",
)
```

Convert [genquery](https://bazel.build/reference/be/general#genquery) output to JSON.
```starlark
genquery(
    name = "deps",
    expression = "deps(//some:target)",
    scope = ["//some:target"],
)

jq(
    name = "deps_json",
    srcs = [":deps"],
    args = [
        "--raw-input",
        "--slurp",
    ],
    filter = "{ deps: split(\\\"\\\\n\\\") | map(select(. | length > 0)) }",
)
```

When Bazel is run with `--stamp`, replace some properties with version control info:
```starlark
jq(
    name = "stamped",
    srcs = ["package.json"],
    filter = "|".join([
        # Don't directly reference $STAMP as it's only set when stamping
        # This 'as' syntax results in $stamp being null in unstamped builds.
        "$ARGS.named.STAMP as $stamp",
        # Provide a default using the "alternative operator" in case $stamp is null.
        ".version = ($stamp[0].BUILD_EMBED_LABEL // \"<unstamped>\")",
    ]),
)
```

jq is exposed as a "Make variable", so you could use it directly from a `genrule` by referencing the toolchain.

```starlark
genrule(
    name = "case_genrule",
    srcs = ["a.json"],
    outs = ["genrule_output.json"],
    cmd = "$(JQ_BIN) '.' $(location a.json) > $@",
    toolchains = ["@jq_toolchains//:resolved_toolchain"],
)
```
"""

load("//lib/private:jq.bzl", _jq_lib = "jq_lib")

_jq_rule = rule(
    attrs = _jq_lib.attrs,
    implementation = _jq_lib.implementation,
    toolchains = ["@aspect_bazel_lib//lib:jq_toolchain_type"],
)

def jq(name, srcs, filter = None, filter_file = None, args = [], out = None, data = [], expand_args = False, **kwargs):
    """Invoke jq with a filter on a set of json input files.

    Args:
        name: Name of the rule
        srcs: List of input files. May be empty.
        data: List of additional files. May be empty.
        filter: Filter expression (https://stedolan.github.io/jq/manual/#Basicfilters).
            Subject to stamp variable replacements, see [Stamping](./stamping.md).
            When stamping is enabled, a variable named "STAMP" will be available in the filter.

            Be careful to write the filter so that it handles unstamped builds, as in the example above.

        filter_file: File containing filter expression (alternative to `filter`)
        args: Additional args to pass to jq
        expand_args: Run bazel's location and make variable expansion on the args.
        out: Name of the output json file; defaults to the rule name plus ".json"
        **kwargs: Other common named parameters such as `tags` or `visibility`
    """
    default_name = name + ".json"
    if not out and not default_name in srcs:
        out = default_name

    _jq_rule(
        name = name,
        srcs = srcs,
        filter = filter,
        filter_file = filter_file,
        args = args,
        out = out,
        expand_args = expand_args,
        data = data,
        **kwargs
    )
