"""Top level definition exports for rules_proto_grpc."""

load("//internal:common.bzl", _bazel_build_rule_common_attrs = "bazel_build_rule_common_attrs")
load("//internal:compile.bzl", _proto_compile = "proto_compile", _proto_compile_attrs = "proto_compile_attrs", _proto_compile_impl = "proto_compile_impl")
load("//internal:filter_files.bzl", _filter_files = "filter_files")
load("//internal:plugin.bzl", _proto_plugin = "proto_plugin")
load("//internal:providers.bzl", _ProtoCompileInfo = "ProtoCompileInfo", _ProtoPluginInfo = "ProtoPluginInfo")

# Export providers
ProtoPluginInfo = _ProtoPluginInfo
ProtoCompileInfo = _ProtoCompileInfo

# Export plugin rule
proto_plugin = _proto_plugin

# Export compile rule implementation and attrs
proto_compile_attrs = _proto_compile_attrs
proto_compile_impl = _proto_compile_impl

# Export compilation function, which can be wrapped by external rules that need more
# pre-configuration than proto_compile_impl alone allows. e.g third party versions of
# doc_template_compile_impl-like rules
proto_compile = _proto_compile

# Export utils
bazel_build_rule_common_attrs = _bazel_build_rule_common_attrs
filter_files = _filter_files
