"""Rules for running JavaScript programs"""

load(
    "//js/private:js_binary.bzl",
    _js_binary = "js_binary",
    _js_test = "js_test",
)
load(
    "//js/private:js_library.bzl",
    _js_library = "js_library",
)
load(
    "//js/private:js_run_binary.bzl",
    _js_run_binary = "js_run_binary",
)
load(
    "//js/private:js_filegroup.bzl",
    _js_filegroup = "js_filegroup",
)
load(
    "//js/private:js_run_devserver.bzl",
    _js_run_devserver = "js_run_devserver",
)
load(
    "//js/private:js_image_layer.bzl",
    _js_image_layer = "js_image_layer",
)

def js_binary(**kwargs):
    _js_binary(
        enable_runfiles = select({
            Label("@aspect_rules_js//js:enable_runfiles"): True,
            "//conditions:default": False,
        }),
        unresolved_symlinks_enabled = select({
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }),
        **kwargs
    )

def js_test(**kwargs):
    _js_test(
        enable_runfiles = select({
            Label("@aspect_rules_js//js:enable_runfiles"): True,
            "//conditions:default": False,
        }),
        unresolved_symlinks_enabled = select({
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }),
        **kwargs
    )

js_run_devserver = _js_run_devserver
js_filegroup = _js_filegroup
js_library = _js_library
js_run_binary = _js_run_binary
js_image_layer = _js_image_layer
