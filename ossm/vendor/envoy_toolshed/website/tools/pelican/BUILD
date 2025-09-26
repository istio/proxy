load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")
load("@website_pip3//:requirements.bzl", "requirement")

py_console_script_binary(
    name = "pelican",
    pkg = "@website_pip3//pelican",
    script = "pelican",
    deps = [
        requirement("cssmin"),
        requirement("libsass"),
        requirement("markdown"),
        requirement("pelican-webassets"),
        requirement("packaging"),
        requirement("pyyaml"),
    ],
    visibility = ["//visibility:public"],
)

py_console_script_binary(
    name = "pelican-quickstart",
    pkg = "@website_pip3//pelican",
    script = "pelican-quickstart",
)

py_console_script_binary(
    name = "pelican-themes",
    pkg = "@website_pip3//pelican",
    script = "pelican-themes",
)
