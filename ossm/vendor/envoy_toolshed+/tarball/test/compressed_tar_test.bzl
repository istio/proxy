"""Tests for `compressed_tar`."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts", "unittest")
load("//tarball:compressed_tar.bzl", "render_mtree")

def _render_mtree_test_impl(ctx):
    env = unittest.begin(ctx)
    mtree = render_mtree({
        "a.txt": ("bazel-out/bin/a.txt", {}),
        "usr/local/bin/tool": ("bazel-out/bin/tool", {"group": "root", "mode": "0755", "user": "root"}),
        "var/lib/app": ("bazel-out/bin/app", {"gid": 101, "group": "app", "mode": "0644", "uid": 101, "user": "app"}),
    })
    asserts.equals(
        env,
        "\n".join([
            "#mtree",
            # `./`-prefixed (bsdtar strips it), parent directories added, in tar order.
            # Without attributes: root, and `pkg_tar`'s default mode.
            "./a.txt type=file uid=0 uname=root gid=0 gname=root mode=0555 time=946684800 content=bazel-out/bin/a.txt",
            "./usr type=dir uid=0 uname=root gid=0 gname=root mode=0755 time=946684800",
            "./usr/local type=dir uid=0 uname=root gid=0 gname=root mode=0755 time=946684800",
            "./usr/local/bin type=dir uid=0 uname=root gid=0 gname=root mode=0755 time=946684800",
            "./usr/local/bin/tool type=file uid=0 uname=root gid=0 gname=root mode=0755 time=946684800 content=bazel-out/bin/tool",
            "./var type=dir uid=0 uname=root gid=0 gname=root mode=0755 time=946684800",
            "./var/lib type=dir uid=0 uname=root gid=0 gname=root mode=0755 time=946684800",
            # An owner set by the `pkg_files`.
            "./var/lib/app type=file uid=101 uname=app gid=101 gname=app mode=0644 time=946684800 content=bazel-out/bin/app",
            "",
        ]),
        mtree,
    )
    return unittest.end(env)

render_mtree_test = unittest.make(_render_mtree_test_impl)

def _duplicate_path_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, "usr/share/demo/hello.txt is in the tarball twice")
    return analysistest.end(env)

duplicate_path_test = analysistest.make(
    _duplicate_path_test_impl,
    expect_failure = True,
)
