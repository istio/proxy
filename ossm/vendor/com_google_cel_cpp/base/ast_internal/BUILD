# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Libraries for the internal C++ representation of a CEL AST.
# Clients should not depend on these directly, they should prefer to use a tool in the /tools
# directory or use the protobuf AST representation.

package(
    # CEL C++ may freely depend on the AST internals, but no clients should use them.
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "ast_impl",
    srcs = ["ast_impl.cc"],
    hdrs = ["ast_impl.h"],
    deps = [
        ":expr",
        "//base:ast",
        "//internal:casts",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "ast_impl_test",
    srcs = ["ast_impl_test.cc"],
    deps = [
        ":ast_impl",
        ":expr",
        "//base:ast",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "expr",
    srcs = ["expr.cc"],
    hdrs = [
        "expr.h",
    ],
    deps = [
        "//common:constant",
        "//common:expr",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "expr_test",
    srcs = [
        "expr_test.cc",
    ],
    deps = [
        ":expr",
        "//common:expr",
        "//internal:testing",
        "@com_google_absl//absl/types:variant",
    ],
)
