load("@com_envoyproxy_protoc_gen_validate//bazel:pgv_proto_library.bzl", "pgv_cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:python_rules.bzl", _py_proto_library = "py_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("@io_bazel_rules_go//go:def.bzl", "go_test")
load("@io_bazel_rules_go//proto:def.bzl", "go_grpc_library", "go_proto_library")
load(
    "//bazel:external_proto_deps.bzl",
    "EXTERNAL_PROTO_CC_BAZEL_DEP_MAP",
    "EXTERNAL_PROTO_GO_BAZEL_DEP_MAP",
)

_PY_PROTO_SUFFIX = "_py_proto"
_CC_PROTO_SUFFIX = "_cc_proto"
_CC_GRPC_SUFFIX = "_cc_grpc"
_GO_PROTO_SUFFIX = "_go_proto"
_GO_IMPORTPATH_PREFIX = "github.com/cncf/xds/go/"

_COMMON_PROTO_DEPS = [
    "@com_google_protobuf//:any_proto",
    "@com_google_protobuf//:descriptor_proto",
    "@com_google_protobuf//:duration_proto",
    "@com_google_protobuf//:empty_proto",
    "@com_google_protobuf//:struct_proto",
    "@com_google_protobuf//:timestamp_proto",
    "@com_google_protobuf//:wrappers_proto",
    "@com_google_googleapis//google/api:http_proto",
    "@com_google_googleapis//google/rpc:status_proto",
    "@com_envoyproxy_protoc_gen_validate//validate:validate_proto",
]

def _proto_mapping(dep, proto_dep_map, proto_suffix):
    mapped = proto_dep_map.get(dep)
    if mapped == None:
        prefix = "@" + Label(dep).workspace_name if not dep.startswith("//") else ""
        return prefix + "//" + Label(dep).package + ":" + Label(dep).name + proto_suffix
    return mapped

def _go_proto_mapping(dep):
    return _proto_mapping(dep, EXTERNAL_PROTO_GO_BAZEL_DEP_MAP, _GO_PROTO_SUFFIX)

def _cc_proto_mapping(dep):
    return _proto_mapping(dep, EXTERNAL_PROTO_CC_BAZEL_DEP_MAP, _CC_PROTO_SUFFIX)

def _xds_cc_grpc_library(name, proto, deps = []):
    cc_grpc_library(
        name = name,
        srcs = [proto],
        deps = deps,
        proto_only = False,
        grpc_only = True,
        visibility = ["//visibility:public"],
    )

def _xds_cc_py_proto_library(
        name,
        visibility = ["//visibility:private"],
        srcs = [],
        deps = [],
        linkstatic = 0,
        has_services = 0):
    relative_name = ":" + name
    proto_library(
        name = name,
        srcs = srcs,
        deps = deps + _COMMON_PROTO_DEPS,
        visibility = visibility,
    )
    cc_proto_library_name = name + _CC_PROTO_SUFFIX
    pgv_cc_proto_library(
        name = cc_proto_library_name,
        linkstatic = linkstatic,
        cc_deps = [_cc_proto_mapping(dep) for dep in deps] + [
            "@com_google_googleapis//google/api:http_cc_proto",
            "@com_google_googleapis//google/api:httpbody_cc_proto",
            "@com_google_googleapis//google/rpc:status_cc_proto",
        ],
        deps = [relative_name],
        visibility = ["//visibility:public"],
    )

    # Uses gRPC implementation of py_proto_library.
    # https://github.com/grpc/grpc/blob/v1.59.1/bazel/python_rules.bzl#L160
    _py_proto_library(
        name = name + _PY_PROTO_SUFFIX,
        # Actual dependencies are resolved automatically from the proto_library dep tree.
        deps = [relative_name],
        visibility = ["//visibility:public"],
    )

    # Optionally define gRPC services
    if has_services:
        cc_grpc_name = name + _CC_GRPC_SUFFIX
        cc_proto_deps = [cc_proto_library_name] + [_cc_proto_mapping(dep) for dep in deps]
        _xds_cc_grpc_library(name = cc_grpc_name, proto = relative_name, deps = cc_proto_deps)

def xds_proto_package(
        name = "pkg",
        srcs = [],
        deps = [],
        has_services = False,
        visibility = ["//visibility:public"]):
    if srcs == []:
        srcs = native.glob(["*.proto"])

    name = "pkg"
    _xds_cc_py_proto_library(
        name = name,
        visibility = visibility,
        srcs = srcs,
        deps = deps,
        has_services = has_services,
    )

    compilers = ["@io_bazel_rules_go//proto:go_proto", "@com_envoyproxy_protoc_gen_validate//bazel/go:pgv_plugin_go"]
    if has_services:
        compilers = ["@io_bazel_rules_go//proto:go_proto", "@io_bazel_rules_go//proto:go_grpc_v2", "@com_envoyproxy_protoc_gen_validate//bazel/go:pgv_plugin_go"]

    # Because RBAC proro depends on googleapis syntax.proto and checked.proto,
    # which share the same go proto library, it causes duplicative dependencies.
    # Thus, we use depset().to_list() to remove duplicated depenencies.
    go_proto_library(
        name = name + _GO_PROTO_SUFFIX,
        compilers = compilers,
        importpath = _GO_IMPORTPATH_PREFIX + native.package_name(),
        proto = name,
        visibility = ["//visibility:public"],
        deps = depset([_go_proto_mapping(dep) for dep in deps] + [
            "@com_envoyproxy_protoc_gen_validate//validate:go_default_library",
            "@org_golang_google_protobuf//types/known/anypb:go_default_library",
            "@org_golang_google_protobuf//types/known/durationpb:go_default_library",
            "@org_golang_google_protobuf//types/known/structpb:go_default_library",
            "@org_golang_google_protobuf//types/known/timestamppb:go_default_library",
            "@org_golang_google_protobuf//types/known/wrapperspb:go_default_library",
        ]).to_list(),
    )

def xds_cc_test(name, **kwargs):
    native.cc_test(
        name = name,
        **kwargs
    )

def xds_go_test(name, **kwargs):
    go_test(
        name = name,
        **kwargs
    )

# Old names for backward compatibility.
# TODO(roth): Remove these once all callers are migrated to the new names.
def udpa_proto_package(srcs = [], deps = [], has_services = False, visibility = ["//visibility:public"]):
    xds_proto_package(srcs = srcs, deps = deps, has_services = has_services, visibility = visibility)

def udpa_cc_test(name, **kwargs):
    xds_cc_test(name, **kwargs)

def udpa_go_test(name, **kwargs):
    xds_go_test(name, **kwargs)

