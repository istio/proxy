"""Bzlmod module extensions"""

load("//:repositories.bzl", "rust_prost_dependencies")

def _rust_ext_impl(module_ctx):
    # This should contain the subset of WORKSPACE.bazel that defines
    # repositories.
    direct_deps = []

    direct_deps.extend(rust_prost_dependencies(bzlmod = True))

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    return module_ctx.extension_metadata(
        root_module_direct_deps = [repo.repo for repo in direct_deps],
        root_module_direct_dev_deps = [],
    )

rust_ext = module_extension(
    doc = "Dependencies for the rules_rust prost extension.",
    implementation = _rust_ext_impl,
)
