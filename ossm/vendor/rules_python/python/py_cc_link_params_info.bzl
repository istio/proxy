"""Public entry point for PyCcLinkParamsInfo."""

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("//python/private:py_cc_link_params_info.bzl", _starlark_PyCcLinkParamsInfo = "PyCcLinkParamsInfo")

PyCcLinkParamsInfo = (
    _starlark_PyCcLinkParamsInfo if (
        config.enable_pystar or config.BuiltinPyCcLinkParamsProvider == None
    ) else config.BuiltinPyCcLinkParamsProvider
)
