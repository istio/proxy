"""Utilities for webdriver repositories"""

load("@apple_support//tools/http_dmg:http_dmg.bzl", "http_dmg")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _build_file_repository_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repository_ctx.file("BUILD.bazel", repository_ctx.read(repository_ctx.path(repository_ctx.attr.build_file)))

build_file_repository = repository_rule(
    doc = "A repository rule for generating external repositories with a specific build file.",
    implementation = _build_file_repository_impl,
    attrs = {
        "build_file": attr.label(
            doc = "The file to use as the BUILD file for this repository.",
            mandatory = True,
            allow_files = True,
        ),
    },
)

_WEBDRIVER_BUILD_CONTENT = """\
filegroup(
    name = "{name}",
    srcs = ["{tool}"],
    data = glob(
        include = [
            "**",
        ],
        exclude = [
            "*.bazel",
            "BUILD",
            "WORKSPACE",
        ],
    ),
    visibility = ["//visibility:public"],
)
"""

def webdriver_repository(
        *,
        name,
        tool,
        urls,
        integrity = "",
        **kwargs):
    """A repository rule for downloading webdriver tools.

    Args:
        name (str): The name of the repository
        tool (str): The name of the webdriver tool being downloaded.
        urls (list[str]): A list of URLs to a file that will be made available to Bazel.
        integrity (str): Expected checksum in Subresource Integrity format of the file downloaded.
        **kwargs (dict): Additional keyword arguments.
    """
    impl_rule = http_archive
    for url in urls:
        if url.endswith(".dmg"):
            impl_rule = http_dmg
            break

    impl_rule(
        name = name,
        urls = urls,
        integrity = integrity,
        build_file_content = _WEBDRIVER_BUILD_CONTENT.format(
            name = name,
            tool = tool,
        ),
        **kwargs
    )
