"Helpers for generating stardoc documentation"

load("@io_bazel_stardoc//stardoc:stardoc.bzl", _stardoc = "stardoc")
load("//lib:write_source_files.bzl", "write_source_files")

def stardoc_with_diff_test(
        name,
        bzl_library_target,
        **kwargs):
    """Creates a stardoc target that can be auto-detected by update_docs to write the generated doc to the source tree and test that it's up to date.

    This is helpful for minimizing boilerplate in repos with lots of stardoc targets.

    Args:
        name: the name of the stardoc file to be written to the current source directory (.md will be appended to the name). Call bazel run on this target to update the file.
        bzl_library_target: the label of the `bzl_library` target to generate documentation for
        **kwargs: additional attributes passed to the stardoc() rule, such as for overriding the templates
    """

    target_compatible_with = kwargs.pop("target_compatible_with", select({
        # stardoc produces different line endings on Windows
        # which makes the diff_test fail
        "@platforms//os:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }))

    # Generate MD from .bzl
    _stardoc(
        name = name,
        out = name + "-docgen.md",
        input = bzl_library_target + ".bzl",
        deps = [bzl_library_target],
        tags = kwargs.pop("tags", []) + ["package:" + native.package_name()],  # Tag the package name which will help us reconstruct the write_source_files label in update_docs
        target_compatible_with = target_compatible_with,
        **kwargs
    )

def update_docs(name = "update", **kwargs):
    """Stamps an executable run for writing all stardocs declared with stardoc_with_diff_test to the source tree.

    This is to be used in tandem with `stardoc_with_diff_test()` to produce a convenient workflow
    for generating, testing, and updating all doc files as follows:

    ``` bash
    bazel build //{docs_folder}/... && bazel test //{docs_folder}/... && bazel run //{docs_folder}:update
    ```

    eg.

    ``` bash
    bazel build //docs/... && bazel test //docs/... && bazel run //docs:update
    ```

    Args:
        name: the name of executable target
        **kwargs: Other common named parameters such as `tags` or `visibility`
    """

    update_files = {}
    for r in native.existing_rules().values():
        if r["generator_function"] == "stardoc_with_diff_test" and r["generator_name"] == r["name"]:
            for tag in r["tags"]:
                if tag.startswith("package:"):
                    stardoc_name = r["name"]
                    source_file_name = stardoc_name + ".md"
                    generated_file_name = stardoc_name + "-docgen.md"
                    update_files[source_file_name] = generated_file_name

    write_source_files(
        name = name,
        files = update_files,
        **kwargs
    )
