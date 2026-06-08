load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")
load("@website_pip3//:requirements.bzl", "requirement")

py_console_script_binary(
    name = "pelican",
    pkg = requirement("pelican"),
    script = "pelican",
    visibility = ["//visibility:public"],
    deps = [
        requirement("cssmin"),
        requirement("libsass"),
        requirement("markdown"),
        requirement("pelican-webassets"),
        requirement("packaging"),
        requirement("pyyaml"),
    ],
)

py_console_script_binary(
    name = "pelican-quickstart",
    pkg = requirement("pelican"),
    script = "pelican-quickstart",
)

py_console_script_binary(
    name = "pelican-themes",
    pkg = requirement("pelican"),
    script = "pelican-themes",
)
