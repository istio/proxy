licenses(["notice"])

genrule(
    name = "nlohmann_json_hpp",
    srcs = ["@com_github_nlohmann_json_single_header//file"],
    outs = ["nlohmann_json.hpp"],
    cmd = "cp $< $@",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "json_util",
    srcs = ["json_util.cc"],
    hdrs = [
        "json_util.h",
        ":nlohmann_json_hpp",
    ],
    copts = ["-UNULL_PLUGIN"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

exports_files([
    "base64.h",
    "json_util.cc",
    "json_util.h",
])
