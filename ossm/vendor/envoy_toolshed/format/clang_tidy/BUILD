filegroup(
    name = "config_default",
    srcs = [
        ".clang-tidy",
        # '//example:clang_tidy_config', # add package specific configs if needed
    ],
)

label_flag(
    name = "config",
    build_setting_default = ":config_default",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "executable_default",
    srcs = [],  # empty list: system clang-tidy
)

label_flag(
    name = "executable",
    build_setting_default = ":executable_default",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "additional_deps_default",
    srcs = [],
)

label_flag(
    name = "additional_deps",
    build_setting_default = ":additional_deps_default",
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "clang_tidy",
    srcs = ["run_clang_tidy.sh"],
    data = [":config"],
    visibility = ["//visibility:public"],
)
