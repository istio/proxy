load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//tarball:compressed_tar.bzl", "compressed_tar")
load(":compressed_tar_test.bzl", "duplicate_path_test", "render_mtree_test")

pkg_files(
    name = "data",
    srcs = ["hello.txt"],
    attributes = pkg_attributes(
        group = "root",
        mode = "0644",
        user = "root",
    ),
    prefix = "usr/share/demo",
)

pkg_files(
    name = "tool",
    srcs = ["tool.sh"],
    attributes = pkg_attributes(mode = "0755"),
    prefix = "usr/local/bin",
    renames = {"tool.sh": "tool"},
)

# No attributes: `pkg_files`' default mode (0644).
pkg_files(
    name = "plain",
    srcs = ["plain.txt"],
)

[
    compressed_tar(
        name = "%s_tar" % compression,
        srcs = [
            ":data",
            ":plain",
            ":tool",
        ],
        out = "test.tar.%s" % ext,
        compression = compression,
        level = level,
    )
    for compression, ext, level in [
        ("gzip", "gz", None),
        ("zstd", "zst", 19),
    ]
]

# The tarballs: layout, modes, owners, mtime, compression format, and contents.
[
    sh_test(
        name = "%s_tar_test" % compression,
        size = "small",
        srcs = ["assert_compressed_tar.sh"],
        data = [
            "hello.txt",
            "plain.txt",
            "tool.sh",
            ":%s_tar" % compression,
            "@bsd_tar_toolchains//:resolved_toolchain",
        ],
        env = {
            "HELLO": "$(rootpath hello.txt)",
            "MAGIC": magic,
            "PLAIN": "$(rootpath plain.txt)",
            "TAR": "$(rootpath @bsd_tar_toolchains//:resolved_toolchain)",
            "TARBALL": "$(rootpath :%s_tar)" % compression,
            "TOOL": "$(rootpath tool.sh)",
            # The listing shows the mtime (2000-01-01) in the local time zone.
            "TZ": "UTC",
        },
        target_compatible_with = select({
            "@platforms//os:linux": [],
            "@platforms//os:macos": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
    )
    for compression, magic in [
        ("gzip", "1f8b"),
        ("zstd", "28b52ffd"),
    ]
]

render_mtree_test(
    name = "render_mtree_test",
)

# The same path from two `pkg_files` fails the analysis.
compressed_tar(
    name = "duplicate_tar",
    srcs = [
        ":data",
        ":data_again",
    ],
    out = "duplicate.tar.gz",
    compression = "gzip",
    tags = ["manual"],
)

pkg_files(
    name = "data_again",
    srcs = ["hello.txt"],
    prefix = "usr/share/demo",
)

duplicate_path_test(
    name = "duplicate_path_test",
    target_under_test = ":duplicate_tar",
)
