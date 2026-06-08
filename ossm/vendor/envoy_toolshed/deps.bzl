load("@aspect_bazel_lib//lib:repositories.bzl", "register_jq_toolchains", "register_yq_toolchains")
load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
load("@rules_perl//perl:deps.bzl", "perl_register_toolchains", "perl_rules_dependencies")
load("@rules_pkg//pkg:deps.bzl", "rules_pkg_dependencies")
load("@rules_python//python:repositories.bzl", "py_repositories")
load("@toolchains_llvm//toolchain:deps.bzl", "bazel_toolchain_dependencies")
load("//compile:libcxx_libs.bzl", "setup_libcxx_libs")
load("//sysroot:sysroot.bzl", "setup_sysroots")
load("//:versions.bzl", "VERSIONS")

def resolve_dependencies(
        cmake_version = None,
        ninja_version = None):
    py_repositories()
    register_jq_toolchains()
    register_yq_toolchains()
    rules_foreign_cc_dependencies(
        register_preinstalled_tools = True,
        register_default_tools = True,
        cmake_version = cmake_version or VERSIONS["cmake"],
        ninja_version = ninja_version or VERSIONS["ninja"],
    )
    rules_pkg_dependencies()
    perl_rules_dependencies()
    perl_register_toolchains()
    bazel_features_deps()
    bazel_toolchain_dependencies()
    setup_libcxx_libs()
    setup_sysroots()
