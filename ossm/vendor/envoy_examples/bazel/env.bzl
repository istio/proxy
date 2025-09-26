load("//bazel:nested.bzl", "load_envoy_nested_examples")

def _impl(ctx):
    need = ctx.os.environ.get("ENVOY_EXAMPLES_LLVM_ENABLED") == "1"
    ctx.file("llvm_flag.bzl", content = "LLVM_ENABLED = %s\n" % need)
    ctx.file("WORKSPACE", content = "")
    ctx.file("BUILD", content = "")

_envoy_examples_env = repository_rule(
    implementation = _impl,
    environ = ["ENVOY_EXAMPLES_LLVM_ENABLED"],
)

def envoy_examples_env():
    _envoy_examples_env(name = "envoy_examples_env")
    load_envoy_nested_examples()
