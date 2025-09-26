"Override default renderer"

load("@aspect_bazel_lib//lib:docs.bzl", _swdt = "stardoc_with_diff_test")

def stardoc_with_diff_test(name, **kwargs):
    _swdt(
        name = name,
        renderer = "//tools:stardoc_renderer",
        **kwargs
    )
