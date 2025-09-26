"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//cargo:defs.bzl", "cargo_build_script")
load("//rust:defs.bzl", "rust_common", "rust_library", "rust_proc_macro")

def _transitive_link_search_paths_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    link_search_path_files = tut[rust_common.dep_info].link_search_path_files.to_list()
    link_search_path_basenames = [f.basename for f in link_search_path_files]

    # Checks that this contains the dep build script, but not the build script
    # of the dep of the proc_macro.
    asserts.equals(env, link_search_path_basenames, ["dep_build_script.linksearchpaths"])

    return analysistest.end(env)

transitive_link_search_paths_test = analysistest.make(_transitive_link_search_paths_test_impl)

def _transitive_link_search_paths_test():
    cargo_build_script(
        name = "proc_macro_build_script",
        srcs = ["proc_macro_build.rs"],
        edition = "2018",
    )

    rust_proc_macro(
        name = "proc_macro",
        srcs = ["proc_macro.rs"],
        edition = "2018",
        deps = [":proc_macro_build_script"],
    )

    cargo_build_script(
        name = "dep_build_script",
        srcs = ["dep_build.rs"],
        edition = "2018",
    )

    rust_library(
        name = "dep",
        srcs = ["dep.rs"],
        edition = "2018",
        proc_macro_deps = [":proc_macro"],
        deps = [":dep_build_script"],
    )

    transitive_link_search_paths_test(
        name = "transitive_link_search_paths_test",
        target_under_test = ":dep",
    )

def transitive_link_search_paths_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _transitive_link_search_paths_test()

    native.test_suite(
        name = name,
        tests = [
            ":transitive_link_search_paths_test",
        ],
    )
