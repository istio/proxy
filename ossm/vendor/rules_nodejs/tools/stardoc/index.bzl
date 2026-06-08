"Wrap stardoc to set our repo-wide defaults"

load("@aspect_bazel_lib//lib:docs.bzl", _stardoc = "stardoc_with_diff_test")

_PKG = "@rules_nodejs//tools/stardoc"

def stardoc(name, **kwargs):
    _stardoc(
        name = name,
        aspect_template = _PKG + ":templates/aspect.vm",
        header_template = _PKG + ":templates/header.vm",
        func_template = _PKG + ":templates/func.vm",
        provider_template = _PKG + ":templates/provider.vm",
        rule_template = _PKG + ":templates/rule.vm",
        **kwargs
    )
