def unpinned_maven_pin_command_alias_impl(rctx):
    rctx.file(
        "BUILD.bazel",
        content = """
package(default_visibility = ["//visibility:public"])

alias(
    name = "pin",
    actual = "%s",
)
""" % str(rctx.attr.alias),
        executable = False,
    )

unpinned_maven_pin_command_alias = repository_rule(
    unpinned_maven_pin_command_alias_impl,
    attrs = {
        "alias": attr.label(),
    },
)
