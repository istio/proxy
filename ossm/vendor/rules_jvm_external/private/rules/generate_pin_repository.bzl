def _generate_pin_repository_impl(repository_ctx):
    repository_ctx.file(
        "BUILD",
        content = """alias(name = "pin", actual = "@{name}//:pin", visibility = ["//visibility:public"])""".format(
            name = repository_ctx.attr.unpinned_name,
        ),
        executable = False,
    )

generate_pin_repository = repository_rule(
    _generate_pin_repository_impl,
    attrs = {
        "unpinned_name": attr.string(),
    },
)
