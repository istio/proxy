"""whl_config_library implementation for WORKSPACE setups."""

load("//python/private:text_util.bzl", "render")
load(":generate_group_library_build_bazel.bzl", "generate_group_library_build_bazel")

def _whl_config_repo_impl(rctx):
    build_file_contents = generate_group_library_build_bazel(
        repo_prefix = rctx.attr.repo_prefix,
        groups = rctx.attr.groups,
    )
    rctx.file("_groups/BUILD.bazel", build_file_contents)
    rctx.file("BUILD.bazel", "")
    rctx.template(
        "config.bzl",
        rctx.attr._config_template,
        substitutions = {
            "%%PACKAGES%%": render.dict(rctx.attr.whl_map or {}, value_repr = lambda x: "None"),
        },
    )

whl_config_repo = repository_rule(
    attrs = {
        "groups": attr.string_list_dict(
            doc = "A mapping of group names to requirements within that group.",
        ),
        "repo_prefix": attr.string(
            doc = "Prefix used for the whl_library created components of each group",
        ),
        "whl_map": attr.string_dict(
            doc = """\
The wheel map where values are json.encoded strings of the whl_map constructed
in the pip.parse tag class.
""",
        ),
        "_config_template": attr.label(
            default = ":config.bzl.tmpl",
        ),
    },
    doc = """
Create a package containing only wrapper py_library and whl_library rules for implementing dependency groups.
This is an implementation detail of dependency groups and should not be used alone.

PRIVATE USE ONLY, only used in WORKSPACE.
    """,
    implementation = _whl_config_repo_impl,
)
