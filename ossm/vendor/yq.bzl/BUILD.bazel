load("@package_metadata//rules:package_metadata.bzl", "package_metadata")

package_metadata(
    name = "package_metadata",
    purl = "pkg:bazel/{}@{}".format(
        module_name(),
        module_version(),
    ),
    visibility = ["//visibility:public"],
)
