"""Analysis tests for getting the link name of a versioned library."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_import")
load("//rust:defs.bzl", "rust_shared_library")

LIBNAMES = ["sterling", "cheryl", "lana", "pam", "malory", "cyril"]

def _is_in_argv(argv, version = None):
    return any(["-ldylib={}{}".format(name, version or "") in argv for name in LIBNAMES])

def _no_version_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    argv = tut.actions[0].argv

    asserts.true(env, _is_in_argv(argv))

    return analysistest.end(env)

def _prefix_version_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    argv = tut.actions[0].argv

    asserts.true(env, _is_in_argv(argv, "3.8"))

    return analysistest.end(env)

def _suffix_version_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    argv = tut.actions[0].argv

    asserts.true(env, _is_in_argv(argv))

    return analysistest.end(env)

no_version_test = analysistest.make(_no_version_test_impl)
prefix_version_test = analysistest.make(_prefix_version_test_impl)
suffix_version_test = analysistest.make(_suffix_version_test_impl)

def _test_linux():
    rust_shared_library(
        name = "linux_no_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_libsterling.so"],
        target_compatible_with = ["@platforms//os:linux"],
    )
    cc_import(
        name = "import_libsterling.so",
        shared_library = "libsterling.so",
    )
    cc_binary(
        name = "libsterling.so",
        srcs = ["b.c"],
        linkshared = True,
    )
    no_version_test(
        name = "linux_no_version_test",
        target_under_test = ":linux_no_version",
        target_compatible_with = ["@platforms//os:linux"],
    )

    rust_shared_library(
        name = "linux_suffix_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_libcheryl.so.3.8", ":import_libcheryl.so"],
        target_compatible_with = ["@platforms//os:linux"],
    )
    cc_import(
        name = "import_libcheryl.so.3.8",
        shared_library = "libcheryl.so.3.8",
    )
    cc_binary(
        name = "libcheryl.so.3.8",
        srcs = ["b.c"],
        linkshared = True,
    )
    cc_import(
        name = "import_libcheryl.so",
        shared_library = "libcheryl.so",
    )
    copy_file(
        name = "copy_unversioned",
        src = ":libcheryl.so.3.8",
        out = "libcheryl.so",
    )
    suffix_version_test(
        name = "linux_suffix_version_test",
        target_under_test = ":linux_suffix_version",
        target_compatible_with = ["@platforms//os:linux"],
    )

    return [
        ":linux_no_version_test",
        ":linux_suffix_version_test",
    ]

def _test_macos():
    rust_shared_library(
        name = "no_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_liblana.dylib"],
        target_compatible_with = ["@platforms//os:macos"],
    )
    cc_import(
        name = "import_liblana.dylib",
        shared_library = "liblana.dylib",
    )
    cc_binary(
        name = "liblana.dylib",
        srcs = ["b.c"],
        linkshared = True,
    )
    no_version_test(
        name = "macos_no_version_test",
        target_under_test = ":no_version",
        target_compatible_with = ["@platforms//os:macos"],
    )

    rust_shared_library(
        name = "prefix_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_libpam3.8.dylib"],
        target_compatible_with = ["@platforms//os:macos"],
    )
    cc_import(
        name = "import_libpam3.8.dylib",
        shared_library = "libpam3.8.dylib",
    )
    cc_binary(
        name = "libpam3.8.dylib",
        srcs = ["b.c"],
        linkshared = True,
    )
    prefix_version_test(
        name = "macos_prefix_version_test",
        target_under_test = ":prefix_version",
        target_compatible_with = ["@platforms//os:macos"],
    )

    return [
        ":macos_no_version_test",
        ":macos_prefix_version_test",
    ]

def _test_windows():
    rust_shared_library(
        name = "windows_no_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_malory.dll"],
        target_compatible_with = ["@platforms//os:windows"],
    )
    cc_import(
        name = "import_malory.dll",
        interface_library = ":malory.lib",
        shared_library = "malory.dll",
    )
    cc_binary(
        name = "malory.dll",
        srcs = ["b.c"],
        linkshared = True,
    )
    native.filegroup(
        name = "malory_interface_lib",
        srcs = [":malory.dll"],
        output_group = "interface_library",
    )
    copy_file(
        name = "copy_malory_interface_lib",
        src = ":malory_interface_lib",
        out = "malory.lib",
    )
    no_version_test(
        name = "windows_no_version_test",
        target_under_test = ":windows_no_version",
        target_compatible_with = ["@platforms//os:windows"],
    )

    rust_shared_library(
        name = "windows_prefix_version",
        srcs = ["a.rs"],
        edition = "2018",
        deps = [":import_cyril3.8.dll"],
        target_compatible_with = ["@platforms//os:windows"],
    )
    cc_import(
        name = "import_cyril3.8.dll",
        interface_library = ":cyril3.8.lib",
        shared_library = "cyril3.8.dll",
    )
    cc_binary(
        name = "cyril3.8.dll",
        srcs = ["b.c"],
        linkshared = True,
    )
    native.filegroup(
        name = "cyril_interface_lib",
        srcs = [":cyril3.8.dll"],
        output_group = "interface_library",
    )
    copy_file(
        name = "copy_cyril_interface_lib",
        src = ":cyril_interface_lib",
        out = "cyril3.8.lib",
    )
    prefix_version_test(
        name = "windows_prefix_version_test",
        target_under_test = ":windows_prefix_version",
        target_compatible_with = ["@platforms//os:windows"],
    )

    return [
        ":windows_no_version_test",
        ":windows_prefix_version_test",
    ]

def versioned_libs_analysis_test_suite(name):
    """Analysis tests for getting the link name of a versioned library.

    Args:
        name: the test suite name
    """
    tests = []
    tests += _test_linux()
    tests += _test_macos()
    tests += _test_windows()

    native.test_suite(
        name = name,
        tests = tests,
    )
