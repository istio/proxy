"""Bzlmod module extensions"""

load("@bazel_features//:features.bzl", "bazel_features")
load("//:repositories.bzl", "rust_wasm_bindgen_dependencies")

def _rust_ext_impl(module_ctx):
    # This should contain the subset of WORKSPACE.bazel that defines
    # repositories.
    direct_deps = []

    direct_deps.extend(rust_wasm_bindgen_dependencies())

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    metadata_kwargs = {
        "root_module_direct_deps": [repo.repo for repo in direct_deps],
        "root_module_direct_dev_deps": [],
    }

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        metadata_kwargs["reproducible"] = True

    return module_ctx.extension_metadata(**metadata_kwargs)

rust_ext = module_extension(
    doc = "Dependencies for the rules_rust wasm_bindgen extension.",
    implementation = _rust_ext_impl,
)
