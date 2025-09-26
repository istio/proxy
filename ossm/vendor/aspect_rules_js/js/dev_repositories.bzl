"""Our "development" dependencies

Users should *not* need to install these. If users see a load()
statement from these, that's a bug in our distribution.
"""

# buildifier: disable=bzl-visibility
load("//js/private:maybe.bzl", http_archive = "maybe_http_archive")

def rules_js_dev_dependencies():
    "Fetch repositories used for developing the rules"
    http_archive(
        name = "io_bazel_rules_go",
        sha256 = "80a98277ad1311dacd837f9b16db62887702e9f1d1c4c9f796d0121a46c8e184",
        urls = ["https://github.com/bazelbuild/rules_go/releases/download/v0.46.0/rules_go-v0.46.0.zip"],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "32938bda16e6700063035479063d9d24c60eda8d79fd4739563f50d331cb3209",
        urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.35.0/bazel-gazelle-v0.35.0.tar.gz"],
    )

    http_archive(
        name = "bazel_skylib",
        sha256 = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68acee3cb94",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz"],
    )

    http_archive(
        name = "bazel_skylib_gazelle_plugin",
        sha256 = "747addf3f508186234f6232674dd7786743efb8c68619aece5fb0cac97b8f415",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-gazelle-plugin-1.5.0.tar.gz"],
    )

    http_archive(
        name = "io_bazel_stardoc",
        sha256 = "3fd8fec4ddec3c670bd810904e2e33170bedfe12f90adf943508184be458c8bb",
        urls = ["https://github.com/bazelbuild/stardoc/releases/download/0.5.3/stardoc-0.5.3.tar.gz"],
    )

    http_archive(
        name = "buildifier_prebuilt",
        sha256 = "72b5bb0853aac597cce6482ee6c62513318e7f2c0050bc7c319d75d03d8a3875",
        strip_prefix = "buildifier-prebuilt-6.3.3",
        urls = ["https://github.com/keith/buildifier-prebuilt/archive/6.3.3.tar.gz"],
    )

    http_archive(
        name = "aspect_rules_lint",
        sha256 = "6e32df708b40ea8c6d4482eeaf8fd6afaa8798d0028654ba25b667b5aee5707c",
        strip_prefix = "rules_lint-0.7.0",
        url = "https://github.com/aspect-build/rules_lint/releases/download/v0.7.0/rules_lint-v0.7.0.tar.gz",
    )

    http_archive(
        name = "chalk_501",
        integrity = "sha256-/nD5GSp77HDNFDwIL68S5PbS+8gefWkube2iIr80/x4=",
        url = "https://registry.npmjs.org/chalk/-/chalk-5.0.1.tgz",
        build_file = "//npm/private/test:vendored/chalk-5.0.1.BUILD",
    )

    http_archive(
        name = "com_grail_bazel_toolchain",
        sha256 = "a9fc7cf01d0ea0a935bd9e3674dd3103766db77dfc6aafcb447a7ddd6ca24a78",
        strip_prefix = "toolchains_llvm-c65ef7a45907016a754e5bf5bfabac76eb702fd3",
        urls = ["https://github.com/bazel-contrib/toolchains_llvm/archive/c65ef7a45907016a754e5bf5bfabac76eb702fd3.tar.gz"],
    )

    http_archive(
        name = "org_chromium_sysroot_linux_arm64",
        build_file_content = _SYSROOT_LINUX_BUILD_FILE,
        sha256 = "cf2fefded0449f06d3cf634bfa94ffed60dbe47f2a14d2900b00eb9bcfb104b8",
        urls = ["https://commondatastorage.googleapis.com/chrome-linux-sysroot/toolchain/80fc74e431f37f590d0c85f16a9d8709088929e8/debian_bullseye_arm64_sysroot.tar.xz"],
    )

    http_archive(
        name = "org_chromium_sysroot_linux_x86_64",
        build_file_content = _SYSROOT_LINUX_BUILD_FILE,
        sha256 = "04b94ba1098b71f8543cb0ba6c36a6ea2890d4d417b04a08b907d96b38a48574",
        urls = ["https://commondatastorage.googleapis.com/chrome-linux-sysroot/toolchain/f5f68713249b52b35db9e08f67184cac392369ab/debian_bullseye_amd64_sysroot.tar.xz"],
    )

    http_archive(
        name = "sysroot_darwin_universal",
        build_file_content = _SYSROOT_DARWIN_BUILD_FILE,
        # The ruby header has an infinite symlink that we need to remove.
        patch_cmds = ["rm System/Library/Frameworks/Ruby.framework/Versions/Current/Headers/ruby/ruby"],
        sha256 = "71ae00a90be7a8c382179014969cec30d50e6e627570af283fbe52132958daaf",
        strip_prefix = "MacOSX11.3.sdk",
        urls = ["https://s3.us-east-2.amazonaws.com/static.aspect.build/sysroots/MacOSX11.3.sdk.tar.xz"],
    )

_SYSROOT_LINUX_BUILD_FILE = """
filegroup(
    name = "sysroot",
    srcs = glob(["*/**"]),
    visibility = ["//visibility:public"],
)
"""

_SYSROOT_DARWIN_BUILD_FILE = """
filegroup(
    name = "sysroot",
    srcs = glob(
        include = ["**"],
        exclude = ["**/*:*"],
    ),
    visibility = ["//visibility:public"],
)
"""
