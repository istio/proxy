"""Tests for `load_arbitrary_tool`"""

load("//rust:repositories.bzl", "load_arbitrary_tool")
load("//rust/platform:triple.bzl", "get_host_triple")
load("//rust/platform:triple_mappings.bzl", "system_to_binary_ext")

def _load_arbitrary_tool_test_impl(repository_ctx):
    host_triple = get_host_triple(repository_ctx)
    cargo_bin = "bin/cargo" + system_to_binary_ext(host_triple.system)

    # Download cargo
    load_arbitrary_tool(
        ctx = repository_ctx,
        tool_name = "cargo",
        tool_subdirectories = ["cargo"],
        version = "1.53.0",
        iso_date = None,
        target_triple = host_triple,
    )

    repo_path = repository_ctx.path(".")
    repository_ctx.file(
        "{}/BUILD.bazel".format(repo_path),
        content = "exports_files([\"{}\"])".format(cargo_bin),
    )

_load_arbitrary_tool_test = repository_rule(
    implementation = _load_arbitrary_tool_test_impl,
    doc = (
        "A test repository rule ensuring `load_arbitrary_tool` functions " +
        "without requiring any attributes on a repository rule"
    ),
)

def load_arbitrary_tool_test():
    """Define the a test repository for ensuring `load_arbitrary_tool` has no attribute requirements"""
    _load_arbitrary_tool_test(
        name = "rules_rust_test_load_arbitrary_tool",
    )
    return [struct(repo = "rules_rust_test_load_arbitrary_tool", is_dev_dep = True)]
