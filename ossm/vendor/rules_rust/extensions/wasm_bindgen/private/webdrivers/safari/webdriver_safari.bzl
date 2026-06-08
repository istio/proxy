"""Depednencies for `wasm_bindgen_test` rules"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_SAFARIDRIVER_WRAPPER_TEMPLATE = """\
#!/usr/bin/env bash

set -euo pipefail

exec {safaridriver} $@
"""

_SAFARIDRIVER_BUILD_CONTENT = """\
exports_files(
    ["safaridriver.sh"],
    visibility = ["//visibility:public"],
)

alias(
    name = "safaridriver",
    actual = "safaridriver.sh",
    target_compatible_with = ["@platforms//os:macos"],
    visibility = ["//visibility:public"],
)
"""

def _safaridriver_repository_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    safaridriver = repository_ctx.os.environ.get(
        "SAFARIDRIVER_BINARY",
        "/usr/bin/safaridriver",
    )

    repository_ctx.file(
        "safaridriver.sh",
        _SAFARIDRIVER_WRAPPER_TEMPLATE.format(
            safaridriver = safaridriver,
        ),
        executable = True,
    )

    repository_ctx.file("BUILD.bazel", _SAFARIDRIVER_BUILD_CONTENT)

safaridriver_repository = repository_rule(
    doc = """\
A repository rule for wrapping the path to a host installed safaridriver binary
""",
    implementation = _safaridriver_repository_impl,
    environ = ["SAFARIDRIVER_BINARY"],
)

def safari_deps():
    """Safari dependencies

    Returns:
        A list of repositories crated
    """

    direct_deps = []

    direct_deps.append(struct(repo = "safaridriver"))
    maybe(
        safaridriver_repository,
        name = "safaridriver",
    )

    return direct_deps
