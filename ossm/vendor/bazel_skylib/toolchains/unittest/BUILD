load("//lib:unittest.bzl", "TOOLCHAIN_TYPE", "unittest_toolchain")

package(default_applicable_licenses = ["//:license"])

licenses(["notice"])

toolchain_type(
    name = "toolchain_type",
    visibility = ["//visibility:public"],
)

unittest_toolchain(
    name = "cmd",
    escape_chars_with = {"%": "%"},
    failure_templ = """@echo off
echo %s
exit /b 1
""",
    file_ext = ".bat",
    join_on = "\necho ",
    success_templ = "@exit /b 0",
    visibility = ["//visibility:public"],
)

toolchain(
    name = "cmd_toolchain",
    exec_compatible_with = [
        "@platforms//os:windows",
    ],
    toolchain = ":cmd",
    toolchain_type = TOOLCHAIN_TYPE,
)

unittest_toolchain(
    name = "bash",
    escape_other_chars_with = "\\",
    failure_templ = """#!/bin/sh
echo %s
exit 1
""",
    file_ext = ".sh",
    join_on = "\necho ",
    success_templ = "#!/bin/sh\nexit 0",
    visibility = ["//visibility:public"],
)

toolchain(
    name = "bash_toolchain",
    toolchain = ":bash",
    toolchain_type = TOOLCHAIN_TYPE,
)

filegroup(
    name = "test_deps",
    testonly = True,
    srcs = [
        "BUILD",
    ],
    visibility = ["//:__subpackages__"],
)

# The files needed for distribution
filegroup(
    name = "distribution",
    srcs = ["BUILD"],
    visibility = [
        "//:__pkg__",
    ],
)
