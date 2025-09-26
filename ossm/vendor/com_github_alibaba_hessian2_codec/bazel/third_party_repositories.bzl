load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def load_third_party_repositories():
    # abseil-cpp
    http_archive(
        name = "com_google_absl",
        urls = ["https://github.com/abseil/abseil-cpp/archive/6f43f5bb398b6685575b36874e36cf1695734df1.tar.gz"],  # 2022-04-04
        strip_prefix = "abseil-cpp-6f43f5bb398b6685575b36874e36cf1695734df1",
        sha256 = "5ca73792af71ab962ee81cdf575f79480704b8fb87e16ca8f1dc1e9b6822611e",
    )

    # Bazel Skylib. Deps of "com_google_absl".
    http_archive(
        name = "bazel_skylib",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz"],
        sha256 = "f7be3474d42aae265405a592bb7da8e171919d74c16f082a5457840f06054728",
    )

    # Bazel platform rules. Deps of "com_google_absl".
    http_archive(
        name = "platforms",
        sha256 = "b601beaf841244de5c5a50d2b2eddd34839788000fa1be4260ce6603ca0d8eb7",
        strip_prefix = "platforms-98939346da932eef0b54cf808622f5bb0928f00b",
        urls = ["https://github.com/bazelbuild/platforms/archive/98939346da932eef0b54cf808622f5bb0928f00b.zip"],
    )

    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/8b6d3f9c4a774bef3081195d422993323b6bb2e0.zip"],  # 2019-03-05
        strip_prefix = "googletest-8b6d3f9c4a774bef3081195d422993323b6bb2e0",
        sha256 = "d21ba93d7f193a9a0ab80b96e8890d520b25704a6fac976fe9da81fffb3392e3",
    )

    http_archive(
        name = "com_github_fmtlib_fmt",
        urls = ["https://github.com/fmtlib/fmt/archive/6.0.0.tar.gz"],
        strip_prefix = "fmt-6.0.0",
        sha256 = "f1907a58d5e86e6c382e51441d92ad9e23aea63827ba47fd647eacc0d3a16c78",
        build_file = "//bazel/external:fmtlib.BUILD",
    )

    http_archive(
        name = "hedron_compile_commands",
        url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/dc36e462a2468bd79843fe5176542883b8ce4abe.tar.gz",
        sha256 = "d63c1573eb1daa4580155a1f0445992878f4aa8c34eb165936b69504a8407662",
        strip_prefix = "bazel-compile-commands-extractor-dc36e462a2468bd79843fe5176542883b8ce4abe",
    )
