# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Bazel rules for building the Honggfuzz binary and the library linked with the
# fuzz test executables.
#
# To use Honggfuzz, the following OS packages need to be installed:
#   * libunwind-dev
#   * libblocksruntime-dev

# Disable the layering check for including the external headers.
package(features = ["-layering_check"])

HF_ARCH = select({
    "@platforms//os:osx": ["-D_HF_ARCH_DARWIN"],
    "//conditions:default": ["-D_HF_ARCH_LINUX"],
})

COMMON_COPTS = [
    "-D_GNU_SOURCE",
] + HF_ARCH + [
    "-fPIC",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wno-override-init",
    "-Wno-initializer-overrides",
    "-Wno-gnu-empty-initializer",
    "-Wno-format-pedantic",
    "-Wno-gnu-statement-expression",
    "-mllvm",
    "-inline-threshold=2000",
    "-fblocks",

    # Do not instrument Honggfuzz itself, in order to avoid recursive
    # instrumentation calls that would crash the fuzz test binary.
    "-fsanitize-coverage=0",
    "-fno-sanitize=all",
]

LIBRARY_COPTS = [
    "-fno-stack-protector",
    "-U_FORTIFY_SOURCE",
    "-D_FORTIFY_SOURCE=0",
]

# Linker options for intercepting common memory operations. Should stay in sync
# with https://github.com/google/honggfuzz/blob/master/hfuzz_cc/hfuzz-cc.c
SYMBOL_WRAP_LINKOPTS = select({
    "@platforms//os:osx": [],
    "//conditions:default": [
        # Intercept common *cmp functions.
        "-Wl,--wrap=strcmp",
        "-Wl,--wrap=strcasecmp",
        "-Wl,--wrap=stricmp",
        "-Wl,--wrap=strncmp",
        "-Wl,--wrap=strncasecmp",
        "-Wl,--wrap=strnicmp",
        "-Wl,--wrap=strstr",
        "-Wl,--wrap=strcasestr",
        "-Wl,--wrap=memcmp",
        "-Wl,--wrap=bcmp",
        "-Wl,--wrap=memmem",
        "-Wl,--wrap=strcpy",
        # Apache httpd
        "-Wl,--wrap=ap_cstr_casecmp",
        "-Wl,--wrap=ap_cstr_casecmpn",
        "-Wl,--wrap=ap_strcasestr",
        "-Wl,--wrap=apr_cstr_casecmp",
        "-Wl,--wrap=apr_cstr_casecmpn",
        # *SSL
        "-Wl,--wrap=CRYPTO_memcmp",
        "-Wl,--wrap=OPENSSL_memcmp",
        "-Wl,--wrap=OPENSSL_strcasecmp",
        "-Wl,--wrap=OPENSSL_strncasecmp",
        "-Wl,--wrap=memcmpct",
        # libXML2
        "-Wl,--wrap=xmlStrncmp",
        "-Wl,--wrap=xmlStrcmp",
        "-Wl,--wrap=xmlStrEqual",
        "-Wl,--wrap=xmlStrcasecmp",
        "-Wl,--wrap=xmlStrncasecmp",
        "-Wl,--wrap=xmlStrstr",
        "-Wl,--wrap=xmlStrcasestr",
        # Samba
        "-Wl,--wrap=memcmp_const_time",
        "-Wl,--wrap=strcsequal",
        # LittleCMS
        "-Wl,--wrap=cmsstrcasecmp",
        # GLib
        "-Wl,--wrap=g_strcmp0",
        "-Wl,--wrap=g_strcasecmp",
        "-Wl,--wrap=g_strncasecmp",
        "-Wl,--wrap=g_strstr_len",
        "-Wl,--wrap=g_ascii_strcasecmp",
        "-Wl,--wrap=g_ascii_strncasecmp",
        "-Wl,--wrap=g_str_has_prefix",
        "-Wl,--wrap=g_str_has_suffix",
        # CUrl
        "-Wl,--wrap=Curl_strcasecompare",
        "-Wl,--wrap=curl_strequal",
        "-Wl,--wrap=Curl_safe_strcasecompare",
        "-Wl,--wrap=Curl_strncasecompare",
        "-Wl,--wrap=curl_strnequal",
    ],
})

cc_library(
    name = "honggfuzz_common",
    srcs = glob(["libhfcommon/*.c"]),
    hdrs = glob(["libhfcommon/*.h"]),
    copts = COMMON_COPTS + LIBRARY_COPTS,
)

LIB_RT = select({
    "@platforms//os:osx": [],
    "//conditions:default": ["-lrt"],
})

cc_library(
    name = "honggfuzz_engine",
    srcs = glob([
        "libhfuzz/*.c",
        "libhfuzz/*.h",
        "*.h",
    ]),
    copts = COMMON_COPTS + LIBRARY_COPTS,
    linkopts = SYMBOL_WRAP_LINKOPTS + [
        "-ldl",
        "-lpthread",
    ] + LIB_RT,
    visibility = ["//visibility:public"],
    deps = [
        ":honggfuzz_common",
    ],
    alwayslink = 1,
)

cc_binary(
    name = "honggfuzz",
    srcs = glob([
        "*.c",
        "*.h",
    ]) + glob([
        "linux/*.c",
        "linux/*.h",
    ]),
    copts = COMMON_COPTS + [
        "-D_HF_LINUX_NO_BFD",
    ],
    includes = [
        ".",
        "linux",
    ],
    # Consider linking statically with
    # -l:libunwind-ptrace.a and -l:libunwind-generic.a.
    linkopts = [
        "-lpthread",
        "-lunwind-ptrace",
        "-lunwind-generic",
        "-lunwind",
    ] + LIB_RT + [
        "-llzma",
        "-Wl,-Bstatic",
        "-lBlocksRuntime",
        "-Wl,-Bdynamic",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
    deps = [
        ":honggfuzz_common",
    ],
)
