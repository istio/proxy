"""Generated definition of buf_proto_lint_test."""

load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(
    "//:defs.bzl",
    "ProtoPluginInfo",
)
load(
    ":buf.bzl",
    "buf_proto_lint_test_impl",
)

buf_proto_lint_test = rule(
    implementation = buf_proto_lint_test_impl,
    attrs = dict(
        protos = attr.label_list(
            providers = [ProtoInfo],
            mandatory = True,
            doc = "List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)",
        ),
        use_rules = attr.string_list(
            default = ["DEFAULT"],
            mandatory = False,
            doc = "List of Buf lint rule IDs or categories to use",
        ),
        except_rules = attr.string_list(
            default = [],
            mandatory = False,
            doc = "List of Buf lint rule IDs or categories to drop",
        ),
        enum_zero_value_suffix = attr.string(
            default = "_UNSPECIFIED",
            mandatory = False,
            doc = "Specify the allowed suffix for the zero enum value",
        ),
        rpc_allow_same_request_response = attr.bool(
            default = False,
            mandatory = False,
            doc = "Allow request and response message to be reused in a single RPC",
        ),
        rpc_allow_google_protobuf_empty_requests = attr.bool(
            default = False,
            mandatory = False,
            doc = "Allow request message to be ``google.protobuf.Empty``",
        ),
        rpc_allow_google_protobuf_empty_responses = attr.bool(
            default = False,
            mandatory = False,
            doc = "Allow response message to be ``google.protobuf.Empty``",
        ),
        service_suffix = attr.string(
            default = "Service",
            mandatory = False,
            doc = "The suffix to allow for services",
        ),
        options = attr.string_list(
            doc = "Extra options to pass to plugins",
        ),
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [
                Label("//buf:lint_plugin"),
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    test = True,
    toolchains = [str(Label("//protobuf:toolchain_type"))],
)
