load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//bazel:toolchains.bzl", "LIBCLANG_CPP", "LIBLLVM")

licenses(["notice"])  # Apache 2

# OSSM: build the prefixer against the host clang toolchain's libstdc++.
#
# Upstream sources libstdc++ from hermetic GCC runtime repos
# (@gcc_runtime_linux_x86_64 / @gcc_runtime_linux_aarch64, selected by the
# :libstdcxx label_flag) and links it with -nostdinc++/-nostdlib++. Those repo
# names are hardcoded per-arch and only cover x86_64 and aarch64. OSSM builds
# this exec-config tool with the host toolchain (@llvm_host), which already
# provides a full host libstdc++ for the build arch (linked statically via the
# toolchain's stdlib="stdc++" setting) -- the same libstdc++ the rest of the
# //:envoy compile uses. Keeping the hermetic GCC runtime repos breaks the
# s390x/ppc builds and the air-gapped vendor snapshot, so we drop that plumbing
# and let the host clang toolchain supply libstdc++ via -stdlib=libstdc++.
cc_binary(
    name = "prefixer",
    srcs = ["prefixer.cpp"],
    additional_linker_inputs = [
        LIBCLANG_CPP,
    ] + ([LIBLLVM] if LIBLLVM else []),
    copts = [
        "-fno-rtti",  # Required by libclang-cpp
        # Use libstdc++ for ABI compatibility with libclang-cpp
        "-stdlib=libstdc++",
    ],
    linkopts = [
        "$(location " + LIBCLANG_CPP + ")",
    ] + (["$(location " + LIBLLVM + ")"] if LIBLLVM else []) + [
        "-Wl,-rpath,$$ORIGIN/../lib",
        # Use libstdc++ for ABI compatibility with libclang-cpp
        "-stdlib=libstdc++",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@llvm_toolchain_llvm//:clang_tooling_headers",
    ],
)
