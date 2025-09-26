"""Transitive rules_mdbook dependencies"""

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")
load("//private/3rdparty/crates:crates.bzl", "crate_repositories")

def rules_mdbook_transitive_deps(rules_rust_deps = True):
    if rules_rust_deps:
        rules_rust_dependencies()
        rust_register_toolchains()

    crate_repositories()
