"""Bzlmod internal extensions"""

load("@bazel_ci_rules//:rbe_repo.bzl", "rbe_preconfig")

def _rust_ext_dev_impl(module_ctx):
    deps = []

    rbe_preconfig(
        name = "buildkite_config",
        toolchain = "ubuntu1804-bazel-java11",
    )

    deps.append(struct(repo = "buildkite_config"))

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    return module_ctx.extension_metadata(
        root_module_direct_deps = [],
        root_module_direct_dev_deps = [repo.repo for repo in deps],
    )

rust_ext_dev = module_extension(
    doc = "Development dependencies for the rules_rust_protobuf extension.",
    implementation = _rust_ext_dev_impl,
)
