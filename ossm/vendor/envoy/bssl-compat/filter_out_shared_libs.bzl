"""Rule to filter out shared libraries from a cc target's dependencies."""

def _filter_out_shared_libs_impl(ctx):
    dep = ctx.attr.dep
    cc_info = dep[CcInfo]

    # Get the original linking context
    linking_context = cc_info.linking_context

    # Filter out shared libraries from linker inputs
    filtered_linker_inputs = []
    for linker_input in linking_context.linker_inputs.to_list():
        # Only keep static libraries
        filtered_libraries = []
        for lib in linker_input.libraries:
            if lib.static_library or lib.pic_static_library:
                filtered_libraries.append(lib)

        if filtered_libraries:
            filtered_linker_inputs.append(
                cc_common.create_linker_input(
                    owner = linker_input.owner,
                    libraries = depset(filtered_libraries),
                    user_link_flags = depset(linker_input.user_link_flags),
                )
            )

    # Create new linking context with filtered inputs
    new_linking_context = cc_common.create_linking_context(
        linker_inputs = depset(filtered_linker_inputs),
    )

    # Create new CcInfo with filtered linking context
    new_cc_info = CcInfo(
        compilation_context = cc_info.compilation_context,
        linking_context = new_linking_context,
    )

    default_info = dep[DefaultInfo]
    return [
        new_cc_info,
        DefaultInfo(
            files = default_info.files,
            data_runfiles = default_info.data_runfiles,
            default_runfiles = default_info.default_runfiles,
        ),
    ]

filter_out_shared_libs = rule(
    implementation = _filter_out_shared_libs_impl,
    attrs = {
        "dep": attr.label(
            mandatory = True,
            providers = [CcInfo],
        ),
    },
    provides = [CcInfo],
)
