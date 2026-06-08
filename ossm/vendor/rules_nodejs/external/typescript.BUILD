load("@aspect_bazel_lib//lib:copy_directory.bzl", "copy_directory")

# Turn a source directory into a TreeArtifact for RBE-compat
copy_directory(
    name = "npm_typescript",
    src = "package",
    # We must give this as the directory in order for it to appear on NODE_PATH
    out = "typescript",
    visibility = ["//visibility:public"],
)
