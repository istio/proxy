"""Test utiltiies for ensuring bzlmod and workspace versions match"""

load("//:version.bzl", "VERSION")
load("//rust:defs.bzl", "rust_test")

def bzl_version_test(name, module_bazel, **kwargs):
    """A test for ensuring a bzlmod repo is in sync with the core rules_rust version.

    Args:
        name (str): The name of the test
        module_bazel (label): The label of a `MODULE.bazel` file.
        **kwargs (dict): Additional keyword arguments.
    """

    rust_test(
        name = name,
        srcs = [Label("//test/bzl_version:bzl_version_test.rs")],
        data = [
            module_bazel,
        ],
        edition = "2021",
        env = {
            "MODULE_BAZEL": "$(rlocationpath {})".format(module_bazel),
            "VERSION": VERSION,
        },
        deps = [
            Label("//rust/runfiles"),
        ],
        **kwargs
    )
