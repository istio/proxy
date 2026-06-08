"python_stdlib_list module extension for use with bzlmod"

load("@bazel_skylib//lib:modules.bzl", "modules")
load("//:deps.bzl", "python_stdlib_list_deps")

python_stdlib_list = modules.as_extension(
    python_stdlib_list_deps,
    doc = "This extension registers python stdlib list dependencies.",
)
