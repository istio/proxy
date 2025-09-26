package(default_visibility = ["//visibility:public"])

# Toolchain type provided by proto_lang_toolchain rule and used by java_proto_library
toolchain_type(name = "toolchain_type")

# Toolchain type provided by proto_lang_toolchain rule and used by java_lite_proto_library
toolchain_type(name = "lite_toolchain_type")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
    visibility = ["//java:__pkg__"],
)
