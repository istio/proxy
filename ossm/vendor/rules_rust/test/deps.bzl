"""A module defining dependencies of the `rules_rust` tests"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//test/generated_inputs:external_repo.bzl", "generated_inputs_in_external_repo")
load("//test/load_arbitrary_tool:load_arbitrary_tool_test.bzl", "load_arbitrary_tool_test")
load("//test/unit/toolchain:toolchain_test_utils.bzl", "rules_rust_toolchain_test_target_json_repository")

_LIBC_BUILD_FILE_CONTENT = """\
load("@rules_rust//rust:defs.bzl", "rust_library")

rust_library(
    name = "libc",
    srcs = glob(["src/**/*.rs"]),
    edition = "2015",
    rustc_flags = [
        # In most cases, warnings in 3rd party crates are not interesting as
        # they're out of the control of consumers. The flag here silences
        # warnings. For more details see:
        # https://doc.rust-lang.org/rustc/lints/levels.html
        "--cap-lints=allow",
    ],
    visibility = ["//visibility:public"],
)
"""

def rules_rust_test_deps(is_bzlmod = False):
    """Load dependencies for rules_rust tests

    Args:
        is_bzlmod (bool): Whether or not the context from which this macro
            is called is bzlmod.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """

    direct_deps = load_arbitrary_tool_test()
    direct_deps.extend(generated_inputs_in_external_repo())

    maybe(
        http_archive,
        name = "libc",
        build_file_content = _LIBC_BUILD_FILE_CONTENT,
        sha256 = "1ac4c2ac6ed5a8fb9020c166bc63316205f1dc78d4b964ad31f4f21eb73f0c6d",
        strip_prefix = "libc-0.2.20",
        urls = [
            "https://mirror.bazel.build/github.com/rust-lang/libc/archive/0.2.20.zip",
            "https://github.com/rust-lang/libc/archive/0.2.20.zip",
        ],
    )

    maybe(
        rules_rust_toolchain_test_target_json_repository,
        name = "rules_rust_toolchain_test_target_json",
        target_json = Label("//test/unit/toolchain:toolchain-test-triple.json"),
    )

    if not is_bzlmod:
        maybe(
            http_archive,
            name = "rules_python",
            sha256 = "690e0141724abb568267e003c7b6d9a54925df40c275a870a4d934161dc9dd53",
            strip_prefix = "rules_python-0.40.0",
            url = "https://github.com/bazelbuild/rules_python/releases/download/0.40.0/rules_python-0.40.0.tar.gz",
        )

        maybe(
            http_archive,
            name = "rules_testing",
            sha256 = "28c2d174471b587bf0df1fd3a10313f22c8906caf4050f8b46ec4648a79f90c3",
            strip_prefix = "rules_testing-0.7.0",
            url = "https://github.com/bazelbuild/rules_testing/releases/download/v0.7.0/rules_testing-v0.7.0.tar.gz",
        )

    direct_deps.extend([
        struct(repo = "libc", is_dev_dep = True),
        struct(repo = "rules_rust_toolchain_test_target_json", is_dev_dep = True),
    ])

    return direct_deps
