load("@rules_cc//cc:extensions.bzl", "compatibility_proxy_repo")
load("@toolchains_llvm//toolchain:rules.bzl", "llvm_toolchain")
load("//:versions.bzl", "VERSIONS")

def setup_llvm_toolchain(llvm_version = None):
    compatibility_proxy_repo()
    llvm_toolchain(
        name = "llvm_toolchain",
        llvm_version = llvm_version or VERSIONS["llvm"],
        cxx_cross_lib = {
            "linux-aarch64": "@libcxx_libs_aarch64",
            "linux-x86_64": "@libcxx_libs_x86_64",
        },
        sysroot = {
            "linux-x86_64": "@sysroot_linux_amd64//:sysroot",
            "linux-aarch64": "@sysroot_linux_arm64//:sysroot",
        },
    )
