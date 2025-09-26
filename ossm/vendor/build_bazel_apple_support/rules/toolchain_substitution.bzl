"""
Register a subsitution, from a variable to a file, for use in arguments in a
target's attributes. Useful for passing custom tools to a compile or link
"""

def _toolchain_substitution(ctx):
    return [
        DefaultInfo(
            files = depset([ctx.file.src]),
        ),
        platform_common.TemplateVariableInfo({
            ctx.attr.var_name: ctx.file.src.path,
        }),
    ]

toolchain_substitution = rule(
    attrs = {
        "var_name": attr.string(
            mandatory = True,
            doc = "Name of the variable to substitute, ex: FOO",
        ),
        "src": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "Source file to substitute",
        ),
    },
    implementation = _toolchain_substitution,
    doc = """
Register a subsitution, from a variable to a file, for use in arguments in a
target's attributes. Useful for passing custom tools to a compile or link.

### Example:

```bzl
load("@build_bazel_apple_support//rules:toolchain_substitution.bzl", "toolchain_substitution")

toolchain_substitution(
    name = "resource_rules",
    src = "resource_rules.plist",
    var_name = "RULES",
)

ios_application(
    ...
    codesignopts = ["--resource-rules=$(RULES)"],
    codesign_inputs = [":resource_rules"],
    toolchains = [":resource_rules"],
)
```
""",
)
