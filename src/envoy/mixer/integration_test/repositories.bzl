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
load("@io_bazel_rules_go//go:def.bzl", "go_repositories", "new_go_repository", "go_repository")
load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogo_proto_repositories")

def go_istio_api_repositories(use_local=False):
    ISTIO_API_BUILD_FILE = """
# build protos from istio.io/api repo

package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//go:def.bzl", "go_prefix", "go_library")

go_prefix("istio.io/api")

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogoslick_proto_library", "gogo_proto_compile")

gogoslick_proto_library(
    name = "mixer/v1",
    importmap = {
        "gogoproto/gogo.proto": "github.com/gogo/protobuf/gogoproto",
        "google/rpc/status.proto": "github.com/googleapis/googleapis/google/rpc",
        "google/protobuf/timestamp.proto": "github.com/gogo/protobuf/types",
        "google/protobuf/duration.proto": "github.com/gogo/protobuf/types",
    },
    imports = [
        "../../external/com_github_gogo_protobuf",
        "../../external/com_github_google_protobuf/src",
        "../../external/com_github_googleapis_googleapis",
    ],
    inputs = [
        "@com_github_google_protobuf//:well_known_protos",
        "@com_github_googleapis_googleapis//:status_proto",
        "@com_github_gogo_protobuf//gogoproto:go_default_library_protos",
    ],
    protos = [
        "mixer/v1/attributes.proto",
        "mixer/v1/check.proto",
        "mixer/v1/report.proto",
        "mixer/v1/service.proto",
    ],
    verbose = 0,
    visibility = ["//visibility:public"],
    with_grpc = True,
    deps = [
        "@com_github_gogo_protobuf//gogoproto:go_default_library",
        "@com_github_gogo_protobuf//sortkeys:go_default_library",
        "@com_github_gogo_protobuf//types:go_default_library",
        "@com_github_googleapis_googleapis//:google/rpc",
    ],
)

DESCRIPTOR_FILE_GROUP = [
    "mixer/v1/config/descriptor/log_entry_descriptor.proto",
    "mixer/v1/config/descriptor/metric_descriptor.proto",
    "mixer/v1/config/descriptor/monitored_resource_descriptor.proto",
    "mixer/v1/config/descriptor/principal_descriptor.proto",
    "mixer/v1/config/descriptor/quota_descriptor.proto",
    "mixer/v1/config/descriptor/value_type.proto",
]

# gogoslick_proto_compile cannot be used here. it generates Equal, Size, and
# MarshalTo methods for google.protobuf.Struct, which we then later replace
# with interface{}. This causes compilation issues.
gogo_proto_compile(
    name = "mixer/v1/config_gen",
    importmap = {
        "google/protobuf/struct.proto": "github.com/gogo/protobuf/types",
        "mixer/v1/config/descriptor/log_entry_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/metric_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/monitored_resource_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/principal_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/quota_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/value_type.proto": "istio.io/api/mixer/v1/config/descriptor",
    },
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    inputs = DESCRIPTOR_FILE_GROUP + [
        "@com_github_google_protobuf//:well_known_protos",
    ],
    protos = [
        "mixer/v1/config/cfg.proto",
    ],
    verbose = 0,
    visibility = ["//visibility:public"],
    with_grpc = False,
)

gogoslick_proto_library(
    name = "mixer/v1/config/descriptor",
    importmap = {
        "google/protobuf/duration.proto": "github.com/gogo/protobuf/types",
    },
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    inputs = [
        "@com_github_google_protobuf//:well_known_protos",
    ],
    protos = DESCRIPTOR_FILE_GROUP,
    verbose = 0,
    visibility = ["//visibility:public"],
    with_grpc = False,
    deps = [
        "@com_github_gogo_protobuf//sortkeys:go_default_library",
        "@com_github_gogo_protobuf//types:go_default_library",
    ],
)

filegroup(
    name = "mixer/v1/config/descriptor_protos",
    srcs = DESCRIPTOR_FILE_GROUP,
    visibility = ["//visibility:public"],
)

genrule(
    name = "mixer/v1/config_fixed",
    srcs = [":mixer/v1/config_gen"],
    outs = ["fixed_cfg.pb.go"],
    cmd = "sed " +
          "-e 's/*google_protobuf.Struct/interface{}/g' " +
          "-e 's/ValueType_VALUE_TYPE_UNSPECIFIED/VALUE_TYPE_UNSPECIFIED/g' " +
          "$(location :mixer/v1/config_gen) | $(location @org_golang_x_tools_imports//:goimports) > $@",
    message = "Applying overrides to cfg proto",
    tools = ["@org_golang_x_tools_imports//:goimports"],
)

filegroup(
    name = "mixer/v1/attributes_file",
    srcs = ["mixer/v1/global_dictionary.yaml"],
    visibility = ["//visibility:public"],
)
"""
    if use_local:
        native.new_local_repository(
            name = "com_github_istio_api",
            build_file_content = ISTIO_API_BUILD_FILE,
            path = "../api",
        )
    else:
      native.new_git_repository(
          name = "com_github_istio_api",
          build_file_content = ISTIO_API_BUILD_FILE,
          commit = "c77e57cdaa2341e41115a8f0dbf18001ad5646c5",
          remote = "https://github.com/istio/api.git",
      )

def go_googleapis_repositories():
    GOOGLEAPIS_BUILD_FILE = """
package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
go_prefix("github.com/googleapis/googleapis")

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogoslick_proto_library")

gogoslick_proto_library(
    name = "google/rpc",
    protos = [
        "google/rpc/code.proto",
        "google/rpc/error_details.proto",
        "google/rpc/status.proto",
    ],
    importmap = {
        "google/protobuf/any.proto": "github.com/gogo/protobuf/types",
        "google/protobuf/duration.proto": "github.com/gogo/protobuf/types",
    },
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    inputs = [
        "@com_github_google_protobuf//:well_known_protos",
    ],
    deps = [
        "@com_github_gogo_protobuf//types:go_default_library",
    ],
    verbose = 0,
)

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

cc_proto_library(
    name = "cc_status_proto",
    protos = [
        "google/rpc/status.proto",
    ],
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    verbose = 0,
)

filegroup(
    name = "status_proto",
    srcs = [ "google/rpc/status.proto" ],
)

filegroup(
    name = "code_proto",
    srcs = [ "google/rpc/code.proto" ],
)
"""
    native.new_git_repository(
        name = "com_github_googleapis_googleapis",
        build_file_content = GOOGLEAPIS_BUILD_FILE,
        commit = "13ac2436c5e3d568bd0e938f6ed58b77a48aba15", # Oct 21, 2016 (only release pre-dates sha)
        remote = "https://github.com/googleapis/googleapis.git",
    )

def go_x_tools_imports_repositories():
    BUILD_FILE = """
package(default_visibility = ["//visibility:public"])
load("@io_bazel_rules_go//go:def.bzl", "go_binary")
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")

go_prefix("golang.org/x/tools")

licenses(["notice"])  # New BSD

exports_files(["LICENSE"])

go_binary(
    name = "goimports",
    srcs = [
        "cmd/goimports/doc.go",
        "cmd/goimports/goimports.go",
        "cmd/goimports/goimports_gc.go",
        "cmd/goimports/goimports_not_gc.go",
    ],
    deps = [
        "@org_golang_x_tools//imports:go_default_library",
    ],
)
"""
    # bazel rule for fixing up cfg.pb.go relies on running goimports
    # we import it here as a git repository to allow projection of a
    # simple build rule that will build the binary for usage (and avoid
    # the need to project a more complicated BUILD file over the entire
    # tools repo.)
    native.new_git_repository(
	name = "org_golang_x_tools_imports",
        build_file_content = BUILD_FILE,
        commit = "e6cb469339aef5b7be0c89de730d5f3cc8e47e50",  # Jun 23, 2017 (no releases)
        remote = "https://github.com/golang/tools.git",
    )

def go_mixer_repositories(use_local_api=False):
    go_repositories()
    gogo_proto_repositories()
    go_x_tools_imports_repositories()
    go_istio_api_repositories(use_local_api)
    go_googleapis_repositories()

    go_repository(
        name = "org_golang_x_text",
        build_file_name = "BUILD.bazel",
        commit = "f4b4367115ec2de254587813edaa901bc1c723a8",  # Mar 31, 2017 (no releases)
        importpath = "golang.org/x/text",
    )

    go_repository(
        name = "org_golang_x_tools",
        commit = "e6cb469339aef5b7be0c89de730d5f3cc8e47e50",  # Jun 23, 2017 (no releases)
        importpath = "golang.org/x/tools",
    )

    go_repository(
        name = "com_github_hashicorp_go_multierror",
        commit = "ed905158d87462226a13fe39ddf685ea65f1c11f",  # Dec 16, 2016 (no releases)
        importpath = "github.com/hashicorp/go-multierror",
    )

    go_repository(
        name = "com_github_hashicorp_errwrap",
        commit = "7554cd9344cec97297fa6649b055a8c98c2a1e55",  # Oct 27, 2014 (no releases)
        importpath = "github.com/hashicorp/errwrap",
    )

    go_repository(
        name = "com_github_istio_mixer",
        commit = "4b3296a43ce940ba47fab7ad35fdf5c0c18778cd",
        importpath = "github.com/istio/mixer",
    )
