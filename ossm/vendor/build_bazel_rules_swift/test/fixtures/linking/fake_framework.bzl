"""Simple rule to emulate apple_static_framework_import"""

load(
    "@bazel_tools//tools/cpp:toolchain_utils.bzl",
    "find_cpp_toolchain",
    "use_cpp_toolchain",
)

def _impl(ctx):
    binary1 = ctx.actions.declare_file("framework1.framework/framework1")
    ctx.actions.write(binary1, "empty")
    binary2 = ctx.actions.declare_file("framework2.framework/framework2")
    ctx.actions.write(binary2, "empty")
    if hasattr(apple_common.new_objc_provider(), "static_framework_file"):
        return apple_common.new_objc_provider(
            static_framework_file = depset([binary1]),
            imported_library = depset([binary1]),
            dynamic_framework_file = depset([binary2]),
        )
    else:
        cc_toolchain = find_cpp_toolchain(ctx)
        feature_configuration = cc_common.configure_features(
            ctx = ctx,
            cc_toolchain = cc_toolchain,
            language = "objc",
            requested_features = ctx.features,
            unsupported_features = ctx.disabled_features,
        )
        return CcInfo(
            linking_context = cc_common.create_linking_context(
                linker_inputs = depset([
                    cc_common.create_linker_input(
                        owner = ctx.label,
                        libraries = depset([
                            cc_common.create_library_to_link(
                                actions = ctx.actions,
                                cc_toolchain = cc_toolchain,
                                feature_configuration = feature_configuration,
                                static_library = binary1,
                            ),
                            cc_common.create_library_to_link(
                                actions = ctx.actions,
                                cc_toolchain = cc_toolchain,
                                dynamic_library = binary2,
                                feature_configuration = feature_configuration,
                            ),
                        ]),
                    ),
                ]),
            ),
        )

fake_framework = rule(
    implementation = _impl,
    attrs = {
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
            doc = "The C++ toolchain to use.",
        ),
    },
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)
