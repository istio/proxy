"""Generated definition of buf_proto_breaking_test."""

load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(
    "//:defs.bzl",
    "ProtoPluginInfo",
)
load(
    ":buf.bzl",
    "buf_proto_breaking_test_impl",
)

buf_proto_breaking_test = rule(
    implementation = buf_proto_breaking_test_impl,
    attrs = dict(
        protos = attr.label_list(
            providers = [ProtoInfo],
            default = [],
            mandatory = True,
            doc = "List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)",
        ),
        against_input = attr.label(
            allow_single_file = [".bin", ".json"],
            mandatory = True,
            doc = "Label of an existing input image file to check against (.json or .bin)",
        ),
        use_rules = attr.string_list(
            default = ["FILE"],
            mandatory = False,
            doc = "List of Buf breaking rule IDs or categories to use",
        ),
        except_rules = attr.string_list(
            default = [],
            mandatory = False,
            doc = "List of Buf breaking rule IDs or categories to drop",
        ),
        ignore_unstable_packages = attr.bool(
            default = False,
            mandatory = False,
            doc = "Whether to ignore breaking changes in unstable package versions",
        ),
        options = attr.string_list(
            doc = "Extra options to pass to plugins",
        ),
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [
                Label("//buf:breaking_plugin"),
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    test = True,
    toolchains = [str(Label("//protobuf:toolchain_type"))],
)
