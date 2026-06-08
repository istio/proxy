"""Bzlmod module extensions that are only used internally"""

load("@bazel_features//:features.bzl", "bazel_features")
load("//crate_universe:deps_bootstrap.bzl", "cargo_bazel_bootstrap")
load("//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("//crate_universe/tools/cross_installer:cross_installer_deps.bzl", "cross_installer_deps")

def _internal_deps_impl(module_ctx):
    direct_deps = []

    direct_deps.extend(crate_universe_dependencies())

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

# This is named a single character to reduce the size of path names when running build scripts, to reduce the chance
# of hitting the 260 character windows path name limit.
# TODO: https://github.com/bazelbuild/rules_rust/issues/1120
cu = module_extension(
    doc = "Dependencies for crate_universe.",
    implementation = _internal_deps_impl,
)

def _internal_non_reproducible_deps_impl(module_ctx):
    direct_deps = []

    direct_deps.extend(cargo_bazel_bootstrap(
        rust_toolchain_cargo_template = "@rust_host_tools//:bin/{tool}",
        rust_toolchain_rustc_template = "@rust_host_tools//:bin/{tool}",
        compressed_windows_toolchain_names = False,
    ))

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    return module_ctx.extension_metadata(
        root_module_direct_deps = [repo.repo for repo in direct_deps],
        root_module_direct_dev_deps = [],
    )

# This is named a single character to reduce the size of path names when running build scripts, to reduce the chance
# of hitting the 260 character windows path name limit.
# TODO: https://github.com/bazelbuild/rules_rust/issues/1120
cu_nr = module_extension(
    doc = "Dependencies for crate_universe (non reproducible).",
    implementation = _internal_non_reproducible_deps_impl,
)

def _internal_dev_deps_impl(module_ctx):
    direct_deps = []

    direct_deps.extend(cross_installer_deps(
        rust_toolchain_cargo_template = "@rust_host_tools//:bin/{tool}",
        rust_toolchain_rustc_template = "@rust_host_tools//:bin/{tool}",
        compressed_windows_toolchain_names = False,
    ))

    # is_dev_dep is ignored here. It's not relevant for internal_deps, as dev
    # dependencies are only relevant for module extensions that can be used
    # by other MODULES.
    return module_ctx.extension_metadata(
        root_module_direct_deps = [],
        root_module_direct_dev_deps = [repo.repo for repo in direct_deps],
    )

# This is named a single character to reduce the size of path names when running build scripts, to reduce the chance
# of hitting the 260 character windows path name limit.
# TODO: https://github.com/bazelbuild/rules_rust/issues/1120
cu_dev = module_extension(
    doc = "Development dependencies for crate_universe.",
    implementation = _internal_dev_deps_impl,
)
