
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_python//python:defs.bzl", "py_library")
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")
load("//:path.bzl", "PATH")

bool_flag(
    name = "use_local_llvm_flag",
    build_setting_default = True,
    visibility = ["//visibility:public"],
)

config_setting(
    name = "use_local_llvm",
    flag_values = {
        ":use_local_llvm_flag": "True",
    },
    constraint_values = [
        "@platforms//os:linux",
    ],
    visibility = ["//visibility:public"],
)

bool_flag(
    name = "use_local_sysroot_flag",
    build_setting_default = True,
    visibility = ["//visibility:public"],
)

config_setting(
    name = "use_local_sysroot",
    flag_values = {
        ":use_local_sysroot_flag": "True",
    },
    constraint_values = [
        "@platforms//os:linux",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "use_hermetic_sysroot",
    flag_values = {
        ":use_local_sysroot_flag": "False",
    },
    constraint_values = [
        "@platforms//os:linux",
    ],
    visibility = ["//visibility:public"],
)

py_library(
    name = "envoy_repo",
    srcs = ["envoy_repo.py"],
    visibility = ["//visibility:public"],
)

py_console_script_binary(
    name = "get_project_json",
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project_data",
    data = [":envoy_repo.py"],
)

genrule(
    name = "generate_release_hash_bin",
    outs = ["generate_release_hash.sh"],
    cmd = """
    echo "
#!/usr/bin/env bash

set -e -o pipefail

git ls-remote --tags https://github.com/envoyproxy/envoy \\
    | grep -E 'refs/tags/v[0-9]+\\.[0-9]+\\.[0-9]+$$' \\
    | sort -u \\
    | sha256sum \\
    | cut -d ' ' -f 1" > $@
    chmod +x $@
    """
)

sh_binary(
    name = "generate_release_hash",
    srcs = [":generate_release_hash_bin"],
    visibility = ["//visibility:public"],
)

# This sets a default hash based on currently visible tagged versions.
# Its very questionably hermetic, making assumptions about git, the repo remotes and so on.
# The general idea here is to make this cache blow any time there are release changes.
# You can use the above sh_binary to generate a custom/correct hash to override below.
genrule(
    name = "default_release_hash",
    outs = ["default_release_hash.txt"],
    cmd = """
    $(location :generate_release_hash) > $@
    """,
    stamp = True,
    tags = ["no-remote-exec"],
    tools = [":generate_release_hash"],
)

label_flag(
    name = "release-hash",
    build_setting_default = ":default_release_hash",
    visibility = ["//visibility:public"],
)

genrule(
    name = "project",
    outs = ["project.json"],
    cmd = """
    $(location :get_project_json) $$(dirname $(location @envoy//:VERSION.txt)) > $@
    """,
    tools = [
        ":get_project_json",
        ":release-hash",
        "@envoy//:VERSION.txt",
        "@envoy//changelogs",
    ],
    visibility = ["//visibility:public"],
)

py_console_script_binary(
    name = "release",
    args = [
        "release",
        PATH,
        "--release-message-path=$(location @envoy//changelogs:summary)",
    ],
    data = [
        ":envoy_repo.py",
        "@envoy//changelogs:summary",
    ],
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project",
)

py_console_script_binary(
    name = "dev",
    args = [
        "dev",
        PATH,
    ],
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project",
    data = [":envoy_repo.py"],
)

py_console_script_binary(
    name = "sync",
    args = [
        "sync",
        PATH,
    ],
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project",
    data = [":envoy_repo.py"],
)

py_console_script_binary(
    name = "publish",
    args = [
        "publish",
        PATH,
    ],
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project",
    data = [":envoy_repo.py"],
)

py_console_script_binary(
    name = "trigger",
    args = [
        "trigger",
        PATH,
    ],
    pkg = "@base_pip3//envoy_base_utils",
    script = "envoy.project",
    data = [":envoy_repo.py"],
)

