load(":clang_runtime_lib.bzl", "clang_runtime_lib")

alias(
    name = "asan",
    actual = select({
        "@platforms//os:linux": ":asan_linux",
        "@platforms//os:macos": ":asan_macos",
    }),
    visibility = ["//visibility:public"],
)

alias(
    name = "ubsan",
    actual = select({
        "@platforms//os:linux": ":ubsan_linux",
        "@platforms//os:macos": ":ubsan_macos",
    }),
    visibility = ["//visibility:public"],
)

clang_runtime_lib(
    name = "asan_linux",
    basenames = [
        # LLVM 15+
        "libclang_rt.asan.so",
        # LLVM 14 and earlier
        "libclang_rt.asan-x86_64.so",
    ],
    tags = ["manual"],
)

clang_runtime_lib(
    name = "asan_macos",
    basenames = ["libclang_rt.asan_osx_dynamic.dylib"],
    tags = ["manual"],
)

clang_runtime_lib(
    name = "ubsan_linux",
    basenames = [
        # LLVM 15+
        "libclang_rt.ubsan_standalone.so",
        # LLVM 14 and earlier
        "libclang_rt.ubsan_standalone-x86_64.so",
    ],
    tags = ["manual"],
)

clang_runtime_lib(
    name = "ubsan_macos",
    basenames = ["libclang_rt.ubsan_osx_dynamic.dylib"],
    tags = ["manual"],
)
