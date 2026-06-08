"""A module defining Rust link time optimization (lto) rules"""

load("//rust/private:utils.bzl", "is_exec_configuration")

_LTO_MODES = [
    # Do nothing, let the user manually handle LTO.
    "manual",
    # Default. No mode has been explicitly set, rustc will do "thin local" LTO
    # between the codegen units of a single crate.
    "unspecified",
    # LTO has been explicitly turned "off".
    "off",
    # Perform "thin" LTO. This is similar to "fat" but takes significantly less
    # time to run, but provides similar performance improvements.
    #
    # See: <http://blog.llvm.org/2016/06/thinlto-scalable-and-incremental-lto.html>
    "thin",
    # Perform "fat"/full LTO.
    "fat",
]

RustLtoInfo = provider(
    doc = "A provider describing the link time optimization setting.",
    fields = {"mode": "string: The LTO mode specified via a build setting."},
)

def _rust_lto_flag_impl(ctx):
    value = ctx.build_setting_value

    if value not in _LTO_MODES:
        msg = "{NAME} build setting allowed to take values [{VALUES}], but was set to: {ACTUAL}".format(
            NAME = ctx.label,
            VALUES = ", ".join(["'{}'".format(m) for m in _LTO_MODES]),
            ACTUAL = value,
        )
        fail(msg)

    return RustLtoInfo(mode = value)

rust_lto_flag = rule(
    doc = "A build setting which specifies the link time optimization mode used when building Rust code. Allowed values are: ".format(_LTO_MODES),
    implementation = _rust_lto_flag_impl,
    build_setting = config.string(flag = True),
)

def _determine_lto_object_format(ctx, toolchain, crate_info):
    """Determines if we should run LTO and what bitcode should get included in a built artifact.

    Args:
        ctx (ctx): The calling rule's context object.
        toolchain (rust_toolchain): The current target's `rust_toolchain`.
        crate_info (CrateInfo): The CrateInfo provider of the target crate.

    Returns:
        string: Returns one of only_object, only_bitcode, object_and_bitcode.
    """

    # Even if LTO is enabled don't use it for actions being built in the exec
    # configuration, e.g. build scripts and proc-macros. This mimics Cargo.
    if is_exec_configuration(ctx):
        return "only_object"

    mode = toolchain.lto.mode

    if mode in ["off", "unspecified"]:
        return "only_object"

    perform_linking = crate_info.type in ["bin", "staticlib", "cdylib"]
    is_dynamic = crate_info.type in ["dylib", "cdylib", "proc-macro"]
    needs_object = perform_linking or is_dynamic

    # At this point we know LTO is enabled, otherwise we would have returned above.

    if not needs_object:
        # If we're building an 'rlib' and LTO is enabled, then we can skip
        # generating object files entirely.
        return "only_bitcode"
    elif crate_info.type in ["dylib", "proc-macro"]:
        # If we're a dylib and we're running LTO, then only emit object code
        # because 'rustc' doesn't currently support LTO with dylibs.
        # proc-macros do not benefit from LTO, and cannot be dynamically linked with LTO.
        return "only_object"
    else:
        return "object_and_bitcode"

def construct_lto_arguments(ctx, toolchain, crate_info):
    """Returns a list of 'rustc' flags to configure link time optimization.

    Args:
        ctx (ctx): The calling rule's context object.
        toolchain (rust_toolchain): The current target's `rust_toolchain`.
        crate_info (CrateInfo): The CrateInfo provider of the target crate.

    Returns:
        list: A list of strings that are valid flags for 'rustc'.
    """
    mode = toolchain.lto.mode

    # The user is handling LTO on their own, don't add any arguments.
    if mode == "manual":
        return []

    format = _determine_lto_object_format(ctx, toolchain, crate_info)
    args = []

    # proc-macros do not benefit from LTO, and cannot be dynamically linked with LTO.
    if mode in ["thin", "fat", "off"] and not is_exec_configuration(ctx) and crate_info.type != "proc-macro":
        args.append("-Clto=%s" % mode)

    if format == "object_and_bitcode":
        # Embedding LLVM bitcode in object files is `rustc's` default.
        pass
    elif format == "only_object":
        args.append("-Cembed-bitcode=no")
    elif format == "only_bitcode":
        args.append("-Clinker-plugin-lto")
    else:
        fail("unrecognized LTO object format %s" % format)

    return args
