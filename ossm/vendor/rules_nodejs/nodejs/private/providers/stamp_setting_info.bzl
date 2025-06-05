"Pass information about stamp between rules"

#Modelled after _GoContextData in rules_go/go/private/context.bzl
StampSettingInfo = provider(
    fields = {
        "value": "Whether stamping is enabled",
    },
)

STAMP_ATTR = attr.label(
    default = "@rules_nodejs//nodejs/stamp:use_stamp_flag",
    providers = [StampSettingInfo],
    doc = """Whether to encode build information into the output. Possible values:
    - `@rules_nodejs//nodejs/stamp:always`:
        Always stamp the build information into the output, even in [--nostamp][stamp] builds.
        This setting should be avoided, since it potentially causes cache misses remote caching for
        any downstream actions that depend on it.
    - `@rules_nodejs//nodejs/stamp:never`:
        Always replace build information by constant values. This gives good build result caching.
    - `@rules_nodejs//nodejs/stamp:use_stamp_flag`:
        Embedding of build information is controlled by the [--[no]stamp][stamp] flag.
        Stamped binaries are not rebuilt unless their dependencies change.
    [stamp]: https://docs.bazel.build/versions/main/user-manual.html#flag--stamp""",
)
