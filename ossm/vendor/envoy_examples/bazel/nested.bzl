
def _example_repo_impl(ctx):
    example_resources = [
        "WORKSPACE",
        "BUILD",
        "bazel",
        "envoy.yaml",
        "example.rst",
        "envoy_filter_http_wasm_updated_example.cc",
        "envoy_filter_http_wasm_example.cc",
        "Dockerfile-proxy",
    ]
    for d in example_resources:
        ctx.symlink(ctx.path(ctx.attr.examples_root).dirname.get_child(ctx.attr.path).get_child(d), d)

example_repository = repository_rule(
    implementation = _example_repo_impl,
    attrs = {
        "examples_root": attr.label(default = "@envoy_examples//:BUILD"),
        "path": attr.string(mandatory=True),
    },
)

def load_envoy_nested_examples():
    example_repository(
        name = "envoy-example-wasmcc",
        path = "wasm-cc",
    )
