
def go_istio_api_repositories():
    ISTIO_API_BUILD_FILE = """
package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")

go_prefix("istio.io/api")

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogoslick_proto_library")

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
        "mixer/v1/quota.proto",
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
    "mixer/v1/config/descriptor/attribute_descriptor.proto",
    "mixer/v1/config/descriptor/label_descriptor.proto",
    "mixer/v1/config/descriptor/log_entry_descriptor.proto",
    "mixer/v1/config/descriptor/metric_descriptor.proto",
    "mixer/v1/config/descriptor/monitored_resource_descriptor.proto",
    "mixer/v1/config/descriptor/principal_descriptor.proto",
    "mixer/v1/config/descriptor/quota_descriptor.proto",
    "mixer/v1/config/descriptor/value_type.proto",
]

gogoslick_proto_library(
    name = "mixer/v1/config",
    importmap = {
        "google/protobuf/struct.proto": "github.com/gogo/protobuf/types",
        "mixer/v1/config/descriptor/log_entry_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/metric_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/monitored_resource_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/principal_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
        "mixer/v1/config/descriptor/quota_descriptor.proto": "istio.io/api/mixer/v1/config/descriptor",
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
    deps = [
        ":mixer/v1/config/descriptor",
        "@com_github_gogo_protobuf//sortkeys:go_default_library",
        "@com_github_gogo_protobuf//types:go_default_library",
        "@com_github_googleapis_googleapis//:google/rpc",
    ],
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
        "@com_github_gogo_protobuf//types:go_default_library",
    ],
)
"""

    native.new_git_repository(
        name = "com_github_istio_api",
        build_file_content = ISTIO_API_BUILD_FILE,
        commit = "2cb09827d7f09a6e88eac2c2249dcb45c5419f09", # Mar. 14, 2017 (no releases)
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
        "google/rpc/status.proto",
        "google/rpc/code.proto",
    ],
    importmap = {
        "google/protobuf/any.proto": "github.com/gogo/protobuf/types",
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
"""

    native.new_git_repository(
        name = "com_github_googleapis_googleapis",
        build_file_content = GOOGLEAPIS_BUILD_FILE,
        commit = "13ac2436c5e3d568bd0e938f6ed58b77a48aba15", # Oct 21, 2016 (only release pre-dates sha)
        remote = "https://github.com/googleapis/googleapis.git",
    )
    
def integration_test_repositories():
    native.git_repository(
        name = "io_bazel_rules_go",
        commit = "9496d79880a7d55b8e4a96f04688d70a374eaaf4", # Jan 16, 2017 (v0.3.3/0.4.0)
        remote = "https://github.com/bazelbuild/rules_go.git",
    )

    native.git_repository(
        name = "org_pubref_rules_protobuf",
        commit = "d42e895387c658eda90276aea018056fcdcb30e4", # Mar 07 2017 (gogo* support)
        remote = "https://github.com/pubref/rules_protobuf",
    )

    go_istio_api_repositories()
    go_googleapis_repositories()
    
