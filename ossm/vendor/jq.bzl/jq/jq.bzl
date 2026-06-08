"Public API for calling jq"

load("//jq/private:jq.bzl", "jq_lib")

jq_rule = rule(
    doc = """Most users should use the `jq` macro instead.""",
    attrs = jq_lib.attrs,
    implementation = jq_lib.implementation,
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

    jq_rule(
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
