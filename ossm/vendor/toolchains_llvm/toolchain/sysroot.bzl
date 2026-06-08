load("@aspect_bazel_lib//lib:repo_utils.bzl", "repo_utils")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "get_auth")

def _sysroot_impl(rctx):
    urls = rctx.attr.urls
    if rctx.attr.url:
        urls = [rctx.attr.url] + urls

    if not urls:
        fail("At least one of url and urls must be provided")

    _, _, archive = urls[0].rpartition("/")

    rctx.download(
        urls,
        archive,
        integrity = rctx.attr.integrity,
        auth = get_auth(rctx, urls),
        sha256 = rctx.attr.sha256,
    )

    # Source directories are more efficient than file groups for 2 reasons:
    #   - They can be symlinked into a local sandbox with a single symlink instead of 1-per-file
    #   - They serve as a signal to the Merkle tree cache machinery since they can be memoized as a single node.
    # Since sysroots are usually a ton of files, it can improve build performance to declare them as source directories.

    # Also, create the BUILD file before extracting because `bsdtar` expects the target
    # directory to exist, and this way Bazel creates it for us without needing `mkdir`.
    rctx.file(
        "sysroot/BUILD.bazel",
        """filegroup(
    name = "sysroot",
    srcs = ["."],
    visibility = ["//visibility:public"],
)""",
    )

    host_bsdtar = Label("@bsd_tar_toolchains_%s//:tar" % repo_utils.platform(rctx))

    cmd = [
        str(rctx.path(host_bsdtar)),
        "--extract",
        "--no-same-owner",
        "--no-same-permissions",
        "--file",
        archive,
        "--directory",
        "sysroot",
        "--strip-components",
        str(rctx.attr.strip_components),
    ]

    for include in rctx.attr.include_patterns:
        cmd.extend(["--include", include])

    for exclude in rctx.attr.exclude_patterns:
        cmd.extend(["--exclude", exclude])

    result = rctx.execute(cmd)
    if result.return_code != 0:
        fail(result.stdout + result.stderr)

    rctx.delete(archive)

    if hasattr(rctx, "repo_metadata"):
        return rctx.repo_metadata(reproducible = True)
    else:
        return None

sysroot = repository_rule(
    implementation = _sysroot_impl,
    attrs = {
        "url": attr.string(),
        "urls": attr.string_list(),
        "strip_components": attr.int(
            doc = "Number of components to strip when extracting (similar to strip_prefix).",
        ),
        "sha256": attr.string(),
        "integrity": attr.string(),
        "include_patterns": attr.string_list(),
        "exclude_patterns": attr.string_list(),
    },
)
