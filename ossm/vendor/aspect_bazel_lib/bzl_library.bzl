"""A library rule and macro for grouping Starlark sources.

Drop-in replacement for bzl_library in bazel_skylib, with exceptions:
- We support .bzl and .star extensions, while bzl_library accepts .bzl and .scl.
"""

load("@bazel_skylib//:bzl_library.bzl", "StarlarkLibraryInfo")

def _bzl_library_impl(ctx):
    deps_files = [x.files for x in ctx.attr.deps]
    all_files = depset(ctx.files.srcs, order = "postorder", transitive = deps_files)
    if not ctx.files.srcs and not deps_files:
        fail("bzl_library rule '%s' has no srcs or deps" % ctx.label)

    return [
        # All dependent files should be listed in both `files` and in `runfiles`;
        # this ensures that a `bzl_library` can be referenced as `data` from
        # a separate program, or from `tools` of a genrule().
        DefaultInfo(
            files = all_files,
            runfiles = ctx.runfiles(transitive_files = all_files),
        ),

        # Interop with @bazel_skylib//:bzl_library
        StarlarkLibraryInfo(
            srcs = ctx.files.srcs,
            transitive_srcs = all_files,
        ),
    ]

bzl_library_rule = rule(
    implementation = _bzl_library_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".bzl", ".star"],
        ),
        "deps": attr.label_list(
            allow_files = [".bzl", ".star"],
        ),
    },
    doc = """Creates a logical collection of Starlark .bzl and .star files.""",
)

def bzl_library(name, srcs = [], deps = [], **kwargs):
    """Wrapper for bzl_library.

    Args:
        name: name

        srcs: List of `.bzl` and `.star` files that are processed to create this target.
        deps: List of other `bzl_library` or `filegroup` targets that are required by the Starlark files listed in `srcs`.
        **kwargs: additional arguments for the bzl_library rule.
    """

    # buildifier: disable=unused-variable
    _ = kwargs.pop("compatible_with", None)
    _ = kwargs.pop("exec_compatible_with", None)
    _ = kwargs.pop("features", None)
    _ = kwargs.pop("target_compatible_with", None)
    bzl_library_rule(
        name = name,
        srcs = srcs,
        deps = deps,
        compatible_with = [],
        exec_compatible_with = [],
        features = [],
        target_compatible_with = [],
        **kwargs
    )

    # validate that public API docs have correct deps, by running the doc extractor over them.
    # TODO(alexeagle): it would be better to attach this as a validation action in the bzl_library_rule
    # but there's no tool available for that since the Java implementation code is only exposed as a
    # native Bazel rule.
    # See bazelbuild/bazel-skylib#568
    if hasattr(native, "starlark_doc_extract") and "/private" not in native.package_name():
        extract_targets = []
        for i, src in enumerate(srcs):
            extract_target = "{}.doc_extract{}".format(name, i if i > 0 else "")
            native.starlark_doc_extract(
                name = extract_target,
                src = src,
                deps = deps,
                testonly = True,
                visibility = ["//visibility:private"],
            )
            extract_targets.append(extract_target)
        native.filegroup(
            name = "{}.docs-as-proto".format(name),
            srcs = extract_targets,
            testonly = True,
        )
