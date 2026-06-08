load("@bazel_skylib//rules:write_file.bzl", "write_file")

write_file(
    name = "generated_file",
    out = "generated_file.txt",
    content = ["Hello world from requests"],
)

filegroup(
    name = "whl_orig",
    srcs = glob(
        ["*.whl"],
        allow_empty = False,
        exclude = ["*-patched-*.whl"],
    ),
)
