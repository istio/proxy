"""rules_mdbook dependencies"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//private:toolchain.bzl", "mdbook_toolchain_repository")

def rules_mdbook_dependencies():
    maybe(
        http_archive,
        name = "rules_rust",
        integrity = "sha256-r09Wyq5QqZpov845sUG1Cd1oVIyCBLmKt6HK/JTVuwI=",
        urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.54.1/rules_rust-v0.54.1.tar.gz"],
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

# buildifier: disable=unnamed-macro
def mdbook_register_toolchains(register_toolchains = True):
    """Register toolchains for mdBook rules.

    Args:
        register_toolchains (bool): Whether or not to register toolchains.

    Returns:
      A list of repos visible to the module through the module extension.
    """
    maybe(
        mdbook_toolchain_repository,
        name = "rules_rust_mdbook_toolchain",
        mdbook = str(Label("//private/3rdparty/crates:mdbook__mdbook")),
    )

    if register_toolchains:
        native.register_toolchains("@rules_rust_mdbook_toolchain//:toolchain")

    return [struct(
        repo = "rules_rust_mdbook_toolchain",
        is_dev_dep = False,
    )]
