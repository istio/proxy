"""Bzlmod test extensions"""

load("@bazel_ci_rules//:rbe_repo.bzl", "rbe_preconfig")
load("//test:deps.bzl", "rules_rust_test_deps")

def _rust_test_impl(module_ctx):
    deps = []

    deps.extend(rules_rust_test_deps())

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

rust_test = module_extension(
    doc = "An extension for tests of rules_rust.",
    implementation = _rust_test_impl,
)
