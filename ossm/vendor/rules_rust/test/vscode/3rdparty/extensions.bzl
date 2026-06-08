"""Bzlmod test extensions"""

load("//test/vscode/3rdparty/crates:crates.bzl", "crate_repositories")

def _vscode_test_impl(module_ctx):
    deps = []

    deps.extend(crate_repositories())

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    return module_ctx.extension_metadata(
        root_module_direct_deps = [],
        root_module_direct_dev_deps = [repo.repo for repo in deps],
    )

vscode_test = module_extension(
    doc = "An extension for vscode tests of rules_rust.",
    implementation = _vscode_test_impl,
)
