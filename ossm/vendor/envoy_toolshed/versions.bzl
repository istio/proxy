SUPPORTED_ARCHES = ["aarch64", "x86_64"]

# This is only used for cross-compilation (toolchains_llvm provides these otherwise)
LLVM_CXX_BUILD = """
filegroup(
    name = "libcxx",
    srcs = [
        "lib/{arch}-unknown-linux-gnu/libc++.a",
        "lib/{arch}-unknown-linux-gnu/libc++abi.a",
        "lib/{arch}-unknown-linux-gnu/libunwind.a",
    ],
    visibility = ["//visibility:public"],
)
filegroup(
    name = "compiler_rt",
    srcs = glob(["lib/clang/*/lib/{arch}-unknown-linux-gnu/libclang_rt.builtins.a"]),
    visibility = ["//visibility:public"],
)
filegroup(
    name = "config_site",
    srcs = ["include/{arch}-unknown-linux-gnu/c++/v1/__config_site"],
    visibility = ["//visibility:public"],
)
"""

LLVM_VERSION = "18.1.8"

VERSIONS = {
    "cmake": "3.23.2",
    "llvm": LLVM_VERSION,
    "ninja": "1.12.0",
    "python": "3.12",
    "bins_release": "0.1.53",
    "msan_libs_sha256": "bf6b15102a4a01ff87b2d5df752c64b66da4889382dad74bbce3b78af8adbce9",
    "tsan_libs_sha256": "844a519a0924401273e9001cc3eff2752535eb165b40e488ba684bdc2d1bac0d",

    "libcxx_libs_sha256": {
        "aarch64": "9c900495dac5c214d135caab02cfcf01806916a8eeb994687b2815ed38a79831",
        "x86_64": "50c7385dd1c17fd3606fa8850cfb1c7d0166ab109e404964f55abd6b508560eb",
    },

    # Glint binary hashes by architecture
    "glint_sha256": {
        "amd64": "6afe5a8b82ccf9926c3bd71032d97fc518e2edb52c8855a8343ba0607424438a",
        "arm64": "5c9d59a2139247152a5e1994fba3a0d06ede3538730c5d22773888927b155914",
    },

    # Sysroot hashes organized by glibc version, stdlib variant, and architecture
    # Format: sysroot_hashes[glibc_version][stdlib_variant][arch]
    # stdlib_variant is either "base" (no libstdc++) or the libstdc++ version (e.g., "13")
    "sysroot_hashes": {
        "2.31": {
            "base": {
                "amd64": "6af386782a378d373ec9a60659b5572f7bfdd9f1087a38e4fc37bbbd43fb85f7",
                "arm64": "067102b9eea033415791fd760c4b0161db97b46994bf6fcef79016e5fe60d8a4",
            },
            "13": {
                "amd64": "e282a461c82892db1c735e4d81beb5e2371d482d54a38a674fc7a5b23f7d5635",
                "arm64": "7ddd3d0ad4c8cc8a00f035ee3c39fdd880eb4f4216d9999c2ce5f948ccb60e28",
            },
        },
        "2.28": {
            "base": {
                "amd64": "3a7d19e3ffe472ff432c2e1bb716d300c52cc88cde1d48549e025d1ef393f1c4",
                "arm64": "6525146364f9dbdcd3dc0e95dbfe4094597d9e757b7ed6907d01611c5d909f88",
            },
            "13": {
                "amd64": "3abe9e8986c3e815166769b2c4c25b7a6aed1db253637e2e6ec188796d1794c8",
                "arm64": "ec5eaac9db93aa626c8fd92d3b6ad5d72786052ce5305c51a524f6dff9e2e01c",
            },
        },
    },

    # external archives
    "aspect_bazel_lib": {
        "type": "github_archive",
        "repo": "aspect-build/bazel-lib",
        "version": "2.16.0",
        "sha256": "092f841dd9ea8e736ea834f304877a25190a762d0f0a6c8edac9f94aac8bbf16",
        "strip_prefix": "bazel-lib-{version}",
        "url": "https://github.com/{repo}/archive/v{version}.tar.gz",
    },
    "bazel_features": {
        "type": "github_archive",
        "repo": "bazel-contrib/bazel_features",
        "strip_prefix": "bazel_features-{version}",
        "version": "1.43.0",
        "sha256": "c26b4e69cf02fea24511a108d158188b9d8174426311aac59ce803a78d107648",
        "url": "https://github.com/{repo}/releases/download/v{version}/bazel_features-v{version}.tar.gz",
    },
    "bazel_skylib": {
        "type": "github_archive",
        "repo": "bazelbuild/bazel-skylib",
        "version": "1.4.2",
        "sha256": "66ffd9315665bfaafc96b52278f57c7e2dd09f5ede279ea6d39b2be471e7e3aa",
        "url": "https://github.com/{repo}/releases/download/{version}/bazel-skylib-{version}.tar.gz",
    },
    "llvm_libcxx_aarch64": {
        "arch": "aarch64",
        "type": "http_archive",
        "repo": "llvm/llvm-project",
        "download_suffix": "linux-gnu",
        "version": LLVM_VERSION,
        "sha256": "dcaa1bebbfbb86953fdfbdc7f938800229f75ad26c5c9375ef242edad737d999",
        "url": "https://github.com/{repo}/releases/download/llvmorg-{version}/clang+llvm-{version}-{arch}-{download_suffix}.tar.xz",
        "strip_prefix": "clang+llvm-{version}-{arch}-linux-gnu/",
        "build_file_content": LLVM_CXX_BUILD,
    },
    "llvm_libcxx_x86_64": {
        "arch": "x86_64",
        "download_suffix": "linux-gnu-ubuntu-18.04",
        "type": "http_archive",
        "repo": "llvm/llvm-project",
        "version": LLVM_VERSION,
        "sha256": "54ec30358afcc9fb8aa74307db3046f5187f9fb89fb37064cdde906e062ebf36",
        "url": "https://github.com/{repo}/releases/download/llvmorg-{version}/clang+llvm-{version}-{arch}-{download_suffix}.tar.xz",
        "strip_prefix": "clang+llvm-{version}-{arch}-linux-gnu-ubuntu-18.04/",
        "build_file_content": LLVM_CXX_BUILD,
    },
    "llvm_source": {
        "type": "github_archive",
        "repo": "llvm/llvm-project",
        "version": "llvmorg-%s" % LLVM_VERSION,
        "sha256": "09c08693a9afd6236f27a2ebae62cda656eba19021ef3f94d59e931d662d4856",
        "url": "https://github.com/{repo}/archive/{version}.tar.gz",
        "strip_prefix": "llvm-project-{version}",
        "build_file_content": """filegroup(name = \"all\", srcs = glob([\"**\"]), visibility = [\"//visibility:public\"])""",
    },
    "rules_python": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_python",
        "version": "1.4.1",
        "sha256": "9f9f3b300a9264e4c77999312ce663be5dee9a56e361a1f6fe7ec60e1beef9a3",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },
    "rules_cc": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_cc",
        "version": "0.2.17",
        "sha256": "283fa1cdaaf172337898749cf4b9b1ef5ea269da59540954e51fba0e7b8f277a",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },
    "rules_foreign_cc": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_foreign_cc",
        "version": "0.14.0",
        "sha256": "e0f0ebb1a2223c99a904a565e62aa285bf1d1a8aeda22d10ea2127591624866c",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },
    "rules_perl": {
        "type": "github_archive",
        "repo": "bazel-contrib/rules_perl",
        "version": "0.4.1",
        "sha256": "e09ba7ab6a52059a5bec71cf9a8a5b4e512c8592eb8d15af94ed59e048a2ec6d",
        "url": "https://github.com/{repo}/archive/refs/tags/{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },
    "rules_pkg": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_pkg",
        "version": "1.1.0",
        "sha256": "b7215c636f22c1849f1c3142c72f4b954bb12bb8dcf3cbe229ae6e69cc6479db",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
    },
    "rules_license": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_license",
        "version": "0.0.7",
        "sha256": "4531deccb913639c30e5c7512a054d5d875698daeb75d8cf90f284375fe7c360",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
    },
    "rules_shell": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_shell",
        "version": "0.6.1",
        "sha256": "e6b87c89bd0b27039e3af2c5da01147452f240f75d505f5b6880874f31036307",
        "url": "https://github.com/{repo}/releases/download/v{version}/{name}-v{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },
    "toolchains_llvm": {
        "type": "github_archive",
        "patch_args": ["-p1"],
        "patches": ["@envoy_toolshed//:patches/toolchains_llvm.patch"],
        "repo": "bazel-contrib/toolchains_llvm",
        "version": "1.6.0",
        "sha256": "2b298a1d7ea99679f5edf8af09367363e64cb9fbc46e0b7c1b1ba2b1b1b51058",
        "url": "https://github.com/{repo}/releases/download/v{version}/{name}-v{version}.tar.gz",
        "strip_prefix": "{name}-v{version}",
    },
}
