load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

[cc_binary(
    name = "libreturn_zero." + x,
    srcs = [
        "return_zero.c",
        "return_zero.h",
    ],
    copts = ["-std=c99"],
    linkopts = ["-Wl,-soname,libreturn_zero.so.2"],
    linkshared = True,
    tags = ["manual"],
) for x in ("so", "so.2")]

cc_library(
    name = "libreturn_zero",
    srcs = [
        ":libreturn_zero.so",
        ":libreturn_zero.so.2",
    ],
    tags = ["manual"],
    visibility = ["//visibility:public"],
)
