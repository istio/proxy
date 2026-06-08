"""Dependencies for Rust Prost rules"""

load("//:repositories.bzl", _rust_prost_dependencies = "rust_prost_dependencies")
load("//private/3rdparty/crates:crates.bzl", "crate_repositories")

def rust_prost_dependencies():
    """Prost repository dependencies."""
    crate_repositories()

    _rust_prost_dependencies()

# buildifier: disable=unnamed-macro
def rust_prost_register_toolchains(register_toolchains = True):
    """Register toolchains for proto compilation.

    Args:
        register_toolchains (bool, optional): Whether or not to register the default prost toolchain.
    """

    if register_toolchains:
        native.register_toolchains(str(Label("//:default_prost_toolchain")))
