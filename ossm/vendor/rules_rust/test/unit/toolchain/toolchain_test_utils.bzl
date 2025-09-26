"""Helpers for testing `rust_toolchain.target_json`"""

def _rules_rust_toolchain_test_target_json_repository_impl(repository_ctx):
    target_json_path = repository_ctx.path(repository_ctx.attr.target_json)
    target_json_content = repository_ctx.read(target_json_path)
    target_json = json.decode(target_json_content)

    repository_ctx.file("BUILD.bazel", """exports_files(["defs.bzl"])""")
    repository_ctx.file("defs.bzl", "TARGET_JSON = {}".format(target_json))
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

rules_rust_toolchain_test_target_json_repository = repository_rule(
    doc = (
        "A repository rule used for converting json files to starlark. This " +
        "rule acts as an example of how users can use json files to represent " +
        "custom rust platforms."
    ),
    implementation = _rules_rust_toolchain_test_target_json_repository_impl,
    attrs = {
        "target_json": attr.label(
            doc = "A custom target specification. For more details see: https://doc.rust-lang.org/rustc/targets/custom.html",
            mandatory = True,
        ),
    },
)
