
load("@com_github_grpc_grpc//bazel:python_rules.bzl", _py_proto_library = "py_proto_library")

load("@com_github_grpc_grpc//bazel:python_rules.bzl", _py_grpc_library = "py_grpc_library")

load("@io_bazel_rules_go//proto:def.bzl", _go_proto_library = "go_proto_library")

load("@io_bazel_rules_go//proto:def.bzl", _go_grpc_library = "go_grpc_library")

load("@io_bazel_rules_go//go:def.bzl", _go_library = "go_library")

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", _cc_grpc_library = "cc_grpc_library")

def proto_library_with_info(**kwargs):
    pass

def moved_proto_library(**kwargs):
    pass

def java_proto_library(**kwargs):
    pass

def java_grpc_library(**kwargs):
    pass

def java_gapic_library(**kwargs):
    pass

def java_gapic_test(**kwargs):
    pass

def java_gapic_assembly_gradle_pkg(**kwargs):
    pass

py_proto_library = _py_proto_library

py_grpc_library = _py_grpc_library

def py_gapic_library(**kwargs):
    pass

def py_test(**kwargs):
    pass

def py_gapic_assembly_pkg(**kwargs):
    pass

def py_import(**kwargs):
    pass

go_proto_library = _go_proto_library

go_grpc_library = _go_grpc_library

go_library = _go_library

def go_test(**kwargs):
    pass

def go_gapic_library(**kwargs):
    pass

def go_gapic_assembly_pkg(**kwargs):
    pass

cc_proto_library = native.cc_proto_library

cc_grpc_library = _cc_grpc_library

def cc_gapic_library(**kwargs):
    pass

def php_proto_library(**kwargs):
    pass

def php_grpc_library(**kwargs):
    pass

def php_gapic_library(**kwargs):
    pass

def php_gapic_assembly_pkg(**kwargs):
    pass

def nodejs_gapic_library(**kwargs):
    pass

def nodejs_gapic_assembly_pkg(**kwargs):
    pass

def ruby_proto_library(**kwargs):
    pass

def ruby_grpc_library(**kwargs):
    pass

def ruby_ads_gapic_library(**kwargs):
    pass

def ruby_cloud_gapic_library(**kwargs):
    pass

def ruby_gapic_assembly_pkg(**kwargs):
    pass

def csharp_proto_library(**kwargs):
    pass

def csharp_grpc_library(**kwargs):
    pass

def csharp_gapic_library(**kwargs):
    pass

def csharp_gapic_assembly_pkg(**kwargs):
    pass
