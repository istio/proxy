load(
    "@com_google_absl//absl:copts/GENERATED_copts.bzl",
    "ABSL_GCC_FLAGS",
)

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "demo",
    srcs = [
        "demo.cc",
    ],
    copts = ABSL_GCC_FLAGS + ["-std=c++17"],
    deps = [
        "@com_alibaba_hessian2_codec//hessian2:codec_impl_lib",
        "@com_alibaba_hessian2_codec//hessian2/basic_codec:basic_codec_lib",
    ],
)
