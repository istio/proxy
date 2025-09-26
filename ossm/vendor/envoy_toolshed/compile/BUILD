load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
load("//:versions.bzl", "VERSIONS")

package(default_visibility = ["//visibility:public"])

SANITIZER_ENV = {
    "CXXFLAGS": "-nostdinc++ -nostdlib++",
    "LDFLAGS": "-nostdlib++",
}

BASE_CMAKE_CACHE = {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "CMAKE_INSTALL_PREFIX": ".",
    "CMAKE_POSITION_INDEPENDENT_CODE": "ON",
    "LLVM_ENABLE_RUNTIMES": "libcxxabi;libcxx;libunwind",
    "LLVM_USE_LINKER": "lld",
    "CMAKE_SHARED_LINKER_FLAGS": "-shared -Wl,-S -fuse-ld=lld",
    "CMAKE_MODULE_LINKER_FLAGS": "-shared -Wl,-S -fuse-ld=lld",
    "CMAKE_EXE_LINKER_FLAGS": "-Wl,-S -fuse-ld=lld",
    "BUILD_SHARED_LIBS": "OFF",
    "LIBCXX_ENABLE_SHARED": "OFF",
    "LIBCXX_ENABLE_STATIC": "ON",
    "LIBCXXABI_ENABLE_SHARED": "OFF",
    "LIBCXXABI_ENABLE_STATIC": "ON",
    "LIBUNWIND_ENABLE_SHARED": "OFF",
    "LIBUNWIND_ENABLE_STATIC": "ON",
}

cmake(
    name = "libcxx_msan",
    cache_entries = BASE_CMAKE_CACHE | {
        "LLVM_USE_SANITIZER": "MemoryWithOrigins",
    },
    env = SANITIZER_ENV,
    lib_source = "@llvm_source//:all",
    out_static_libs = [
        "libc++.a",
        "libc++abi.a",
    ],
    tags = ["manual"],
    working_directory = "runtimes",
)

cmake(
    name = "libcxx_tsan",
    cache_entries = BASE_CMAKE_CACHE | {
        "LLVM_USE_SANITIZER": "Thread",
    },
    env = SANITIZER_ENV,
    lib_source = "@llvm_source//:all",
    out_static_libs = [
        "libc++.a",
        "libc++abi.a",
    ],
    tags = ["manual"],
    working_directory = "runtimes",
)

genrule(
    name = "cxx_msan",
    srcs = [":libcxx_msan"],
    outs = ["msan-llvm%s-x86_64.tar.xz" % VERSIONS["llvm"]],
    cmd = """
        mkdir -p msan-libs-x86_64/lib
        for f in $(locations :libcxx_msan); do
            if [[ $$f == *.a ]]; then
                cp $$f msan-libs-x86_64/lib/
            fi
        done
        tar cJf $@ msan-libs-x86_64
    """,
    tags = ["manual"],
)

genrule(
    name = "cxx_tsan",
    srcs = [":libcxx_tsan"],
    outs = ["tsan-llvm%s-x86_64.tar.xz" % VERSIONS["llvm"]],
    cmd = """
        mkdir -p tsan-libs-x86_64/lib
        for f in $(locations :libcxx_tsan); do
            if [[ $$f == *.a ]]; then
                cp $$f tsan-libs-x86_64/lib/
            fi
        done
        tar cJf $@ tsan-libs-x86_64
    """,
    tags = ["manual"],
)
