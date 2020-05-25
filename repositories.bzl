# Copyright 2017 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load(":x_tools_imports.bzl", "go_x_tools_imports_repositories")

GOOGLETEST = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0"
GOOGLETEST_SHA256 = "01508c8f47c99509130f128924f07f3a60be05d039cff571bb11d60bb11a3581"

def googletest_repositories(bind = True):
    BUILD = """
# Copyright 2017 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#
cc_library(
    name = "googletest",
    srcs = [
        "googletest/src/gtest-all.cc",
        "googlemock/src/gmock-all.cc",
    ],
    hdrs = glob([
        "googletest/include/**/*.h",
        "googlemock/include/**/*.h",
        "googletest/src/*.cc",
        "googletest/src/*.h",
        "googlemock/src/*.cc",
    ]),
    includes = [
        "googlemock",
        "googletest",
        "googletest/include",
        "googlemock/include",
    ],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "googletest_main",
    srcs = ["googlemock/src/gmock_main.cc"],
    visibility = ["//visibility:public"],
    deps = [":googletest"],
)
cc_library(
    name = "googletest_prod",
    hdrs = [
        "googletest/include/gtest/gtest_prod.h",
    ],
    includes = [
        "googletest/include",
    ],
    visibility = ["//visibility:public"],
)
"""
    http_archive(
        name = "googletest_git",
        build_file_content = BUILD,
        strip_prefix = "googletest-" + GOOGLETEST,
        url = "https://github.com/google/googletest/archive/" + GOOGLETEST + ".tar.gz",
        sha256 = GOOGLETEST_SHA256,
    )

    if bind:
        native.bind(
            name = "googletest",
            actual = "@googletest_git//:googletest",
        )

        native.bind(
            name = "googletest_main",
            actual = "@googletest_git//:googletest_main",
        )

        native.bind(
            name = "googletest_prod",
            actual = "@googletest_git//:googletest_prod",
        )

#
# To update these...
# 1) find the ISTIO_API SHA you want in git
# 2) wget https://github.com/istio/api/archive/$ISTIO_API_SHA.tar.gz && sha256sum $ISTIO_API_SHA.tar.gz
#
ISTIO_API = "31d048906d97fb7f6b1fa8e250d3fa07456c5acc"
ISTIO_API_SHA256 = "5bf68ef13f4b9e769b7ca0a9ce83d9da5263eed9b1223c4cbb388a6ad5520e01"
GOGOPROTO_RELEASE = "1.2.1"
GOGOPROTO_SHA256 = "99e423905ba8921e86817607a5294ffeedb66fdd4a85efce5eb2848f715fdb3a"

def mixerapi_repositories(bind = True):
    BUILD = """
# Copyright 2018 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

proto_library(
    name = "mixer_api_protos_lib",
    srcs = glob(
        [
            "mixer/v1/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "mixer_api_cc_proto",
    deps = [
        ":mixer_api_protos_lib",
    ],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "mixer_client_config_proto_lib",
    srcs = glob(
        [
        "mixer/v1/config/client/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        ":mixer_api_protos_lib",
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_googleapis//google/api:field_behavior_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "mixer_client_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":mixer_client_config_proto_lib",
    ],
)

proto_library(
    name = "authentication_policy_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/authn/v2alpha1/*.proto",
         "authentication/v1alpha1/*.proto",
         "common/v1alpha1/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_googleapis//google/api:field_behavior_proto",
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "authentication_policy_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":authentication_policy_config_proto_lib",
    ],
)

proto_library(
    name = "jwt_auth_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/jwt_auth/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "jwt_auth_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":jwt_auth_config_proto_lib",
    ],
)

proto_library(
    name = "alpn_filter_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/alpn/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "alpn_filter_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":alpn_filter_config_proto_lib",
    ],
)

proto_library(
    name = "tcp_cluster_rewrite_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "tcp_cluster_rewrite_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":tcp_cluster_rewrite_config_proto_lib",
    ],
)

filegroup(
    name = "global_dictionary_file",
    srcs = ["mixer/v1/global_dictionary.yaml"],
    visibility = ["//visibility:public"],
)

"""
    GOGOPROTO_BUILD = """
load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

proto_library(
    name = "gogo_proto",
    srcs = ["gogoproto/gogo.proto"],
    deps = ["@com_google_protobuf//:descriptor_proto"],
    visibility = ["//visibility:public"],
)

go_proto_library(
    name = "descriptor_go_proto",
    importpath = "github.com/golang/protobuf/protoc-gen-go/descriptor",
    proto = "@com_google_protobuf//:descriptor_proto",
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "gogo_proto_cc",
    deps = [":gogo_proto"],
    visibility = ["//visibility:public"],
)

go_proto_library(
    name = "gogo_proto_go",
    importpath = "gogoproto",
    proto = ":gogo_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":descriptor_go_proto",
    ],
)

py_proto_library(
    name = "gogo_proto_py",
    srcs = [
        "gogoproto/gogo.proto",
    ],
    default_runtime = "@com_google_protobuf//:protobuf_python",
    protoc = "@com_google_protobuf//:protoc",
    visibility = ["//visibility:public"],
    deps = ["@com_google_protobuf//:protobuf_python"],
)
"""
    http_archive(
        name = "com_github_gogo_protobuf",
        build_file_content = GOGOPROTO_BUILD,
        strip_prefix = "protobuf-" + GOGOPROTO_RELEASE,
        url = "https://github.com/gogo/protobuf/archive/v" + GOGOPROTO_RELEASE + ".tar.gz",
        sha256 = GOGOPROTO_SHA256,
    )
    http_archive(
        name = "mixerapi_git",
        build_file_content = BUILD,
        strip_prefix = "api-" + ISTIO_API,
        url = "https://github.com/istio/api/archive/" + ISTIO_API + ".tar.gz",
        sha256 = ISTIO_API_SHA256,
    )
    if bind:
        native.bind(
            name = "mixer_api_cc_proto",
            actual = "@mixerapi_git//:mixer_api_cc_proto",
        )
        native.bind(
            name = "mixer_client_config_cc_proto",
            actual = "@mixerapi_git//:mixer_client_config_cc_proto",
        )
        native.bind(
            name = "authentication_policy_config_cc_proto",
            actual = "@mixerapi_git//:authentication_policy_config_cc_proto",
        )
        native.bind(
            name = "jwt_auth_config_cc_proto",
            actual = "@mixerapi_git//:jwt_auth_config_cc_proto",
        )
        native.bind(
            name = "alpn_filter_config_cc_proto",
            actual = "@mixerapi_git//:alpn_filter_config_cc_proto",
        )
        native.bind(
            name = "tcp_cluster_rewrite_config_cc_proto",
            actual = "@mixerapi_git//:tcp_cluster_rewrite_config_cc_proto",
        )

def wasm_dependencies():
    protobuf_dependencies()
    zlib_dependencies()
    proxy_wasm_cpp_sdk_dependencies()
    emscripten_toolchain_dependencies()
    abseil_dependencies()

def mixerapi_dependencies():
    go_x_tools_imports_repositories()
    mixerapi_repositories()

def docker_dependencies():
    http_archive(
        name = "io_bazel_rules_docker",
        sha256 = "413bb1ec0895a8d3249a01edf24b82fd06af3c8633c9fb833a0cb1d4b234d46d",
        strip_prefix = "rules_docker-0.12.0",
        urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.12.0/rules_docker-v0.12.0.tar.gz"],
    )

def emscripten_toolchain_dependencies():
    BUILD = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])"""
    http_archive(
        name = "emscripten_toolchain",
        sha256 = "4ac0f1f3de8b3f1373d435cd7e58bd94de4146e751f099732167749a229b443b",
        build_file_content = BUILD,
        patch_cmds = [
            "./emsdk install 1.39.6-upstream",
            "./emsdk activate --embedded 1.39.6-upstream",
        ],
        strip_prefix = "emsdk-1.39.6",
        urls = ["https://github.com/emscripten-core/emsdk/archive/1.39.6.tar.gz"],
    )

def proxy_wasm_cpp_sdk_dependencies():
    http_archive(
        name = "proxy_wasm_cpp_sdk",
        sha256 = "3531281b8190ff532b730e92c1f247a2b87995f17a4fd9eaf2ebac6136fbc308",
        strip_prefix = "proxy-wasm-cpp-sdk-96927d814b3ec14893b56793e122125e095654c7",
        urls = ["https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/archive/96927d814b3ec14893b56793e122125e095654c7.tar.gz"],
    )

# This repository is needed to use abseil when wasm build.
# Now we can't use dependencies on external package with wasm. We should implement like `envoy_wasm_cc_binary` to utilize
# dependencies of envoy package.
def abseil_dependencies():
    http_archive(
        name = "com_google_abseil",
        sha256 = "2693730730247afb0e7cb2d41664ac2af3ad75c79944efd266be40ba944179b9",
        strip_prefix = "abseil-cpp-06f0e767d13d4d68071c4fc51e25724e0fc8bc74",
        # 2020-03-03
        urls = ["https://github.com/abseil/abseil-cpp/archive/06f0e767d13d4d68071c4fc51e25724e0fc8bc74.tar.gz"],
    )

# The build strategy to build zlib which is depend by protobuf is not appropriate for emscripten. We should override to
# use pure build strategy for protobuf
def protobuf_dependencies():
    git_repository(
        name = "com_google_protobuf",
        remote = "https://github.com/protocolbuffers/protobuf",
        commit = "655310ca192a6e3a050e0ca0b7084a2968072260",
    )

# This is a dependency of protobuf. In general, the configuration which is defined in protobuf. But, we can't utilize it because
# bazel will use envoy's definition of zlib build. It will cause confusing behavior of emscripten. Specifically, emcc.py will put out
# the failure message of interpreting `libz.so.1.2.11.1-motley`. It is because of invalid prefix of the name of shared library. emcc can't
# regard it as valid shared library. So we decided to introduce the phase of zlib build directly.
def zlib_dependencies():
    ZLIB_BUILD = """
load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # BSD/MIT-like license (for zlib)

_ZLIB_HEADERS = [
    "crc32.h",
    "deflate.h",
    "gzguts.h",
    "inffast.h",
    "inffixed.h",
    "inflate.h",
    "inftrees.h",
    "trees.h",
    "zconf.h",
    "zlib.h",
    "zutil.h",
]

_ZLIB_PREFIXED_HEADERS = ["zlib/include/" + hdr for hdr in _ZLIB_HEADERS]

# In order to limit the damage from the `includes` propagation
# via `:zlib`, copy the public headers to a subdirectory and
# expose those.
genrule(
    name = "copy_public_headers",
    srcs = _ZLIB_HEADERS,
    outs = _ZLIB_PREFIXED_HEADERS,
    cmd = "cp $(SRCS) $(@D)/zlib/include/",
)

cc_library(
    name = "zlib",
    srcs = [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "deflate.c",
        "gzclose.c",
        "gzlib.c",
        "gzread.c",
        "gzwrite.c",
        "infback.c",
        "inffast.c",
        "inflate.c",
        "inftrees.c",
        "trees.c",
        "uncompr.c",
        "zutil.c",
        # Include the un-prefixed headers in srcs to work
        # around the fact that zlib isn't consistent in its
        # choice of <> or "" delimiter when including itself.
    ] + _ZLIB_HEADERS,
    hdrs = _ZLIB_PREFIXED_HEADERS,
    copts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            "-Wno-unused-variable",
            "-Wno-implicit-function-declaration",
        ],
    }),
    includes = ["zlib/include/"],
    visibility = ["//visibility:public"],
)
"""

    http_archive(
        name = "zlib",
        build_file_content = ZLIB_BUILD,
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        strip_prefix = "zlib-1.2.11",
        urls = [
            "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
            "https://zlib.net/zlib-1.2.11.tar.gz",
        ],
    )
