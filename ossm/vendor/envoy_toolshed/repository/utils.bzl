"""Repository rule for defining a host platform based on CPU architecture.

This allows you to set a single alias for multiple architectures, that resolve
to arch-specific targets. Using `native.alias` and `select` for this doesn't work
when using an arch as the selector.

This can be useful specifically for setting the `host_platform`.

Example usage:

In WORKSPACE:

```starlark
arch_alias(
    name = "clang_platform",
    aliases = {
        "amd64": "@envoy//bazel/platforms/rbe:rbe_linux_x64_clang_platform",
        "aarch64": "@envoy//bazel/platforms/rbe:rbe_linux_arm64_clang_platform",
    },
)
```
And then in .bazelrc:

```
common:clang-common --host_platform=@clang_platform
```

"""

ERROR_UNSUPPORTED = """
Unsupported host architecture '{arch}'. Supported architectures are: {supported}
"""

ALIAS_BUILD = """
alias(
    name = "{name}",
    actual = "{actual}",
    visibility = ["//visibility:public"],
)
"""

def _alias_name(name):
    """Extract the appropriate alias name from the repository context.

    In bzlmod, repository names include the canonical name
    (e.g., "module++ext+name"), but we want the alias target to use
    just the apparent name (e.g., "name"). In WORKSPACE mode, the
    repository name is already the apparent name.

    Args:
        ctx: The repository rule context

    Returns:
        The apparent name to use for the alias target
    """
    if not "++" in name and not "~" in name:
        return name
    name = name.replace("++", "+").replace("~~", "~")
    if "+" in name:
        return name.split("+")[-1]
    elif "~" in name:
        return name.split("~")[-1]

def _arch_alias_impl(ctx):
    arch = ctx.os.arch
    actual = ctx.attr.aliases.get(arch)
    if not actual:
        fail(ERROR_UNSUPPORTED.format(
            arch = arch,
            supported = ctx.attr.aliases.keys(),
        ))
    ctx.file(
        "BUILD.bazel",
        ALIAS_BUILD.format(
            name = _alias_name(ctx.name),
            actual = actual,
        ),
    )

arch_alias = repository_rule(
    implementation = _arch_alias_impl,
    attrs = {
        "aliases": attr.string_dict(
            doc = "A dictionary of arch strings, mapped to associated aliases",
        ),
    },
)
