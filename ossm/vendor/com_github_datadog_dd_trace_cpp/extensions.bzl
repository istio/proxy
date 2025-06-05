load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _non_module_dependencies_impl(_ctx):
    http_archive(
        name = "com_google_absl",
        patch_args = ["-p1"],
        patches = ["//:abseil.patch"],
        sha256 = "aabf6c57e3834f8dc3873a927f37eaf69975d4b28117fc7427dfb1c661542a87",
        strip_prefix = "abseil-cpp-98eb410c93ad059f9bba1bf43f5bb916fc92a5ea",
        urls = ["https://github.com/abseil/abseil-cpp/archive/98eb410c93ad059f9bba1bf43f5bb916fc92a5ea.zip"],
    )

non_module_dependencies = module_extension(
    implementation = _non_module_dependencies_impl,
)
