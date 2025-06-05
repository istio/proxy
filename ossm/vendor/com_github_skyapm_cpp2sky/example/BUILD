load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "sample",
    srcs = ["sample.cc"],
    deps = [
        "//cpp2sky:cpp2sky_interface",
        "//source:cpp2sky_lib",
        "@com_github_httplib//:httplib",
    ],
)

cc_binary(
    name = "sample_client",
    srcs = ["sample_client.cc"],
    deps = [
        "//cpp2sky:cpp2sky_interface",
        "//source:cpp2sky_lib",
        "@com_github_httplib//:httplib",
    ],
)
