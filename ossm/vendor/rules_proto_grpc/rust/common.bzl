"""Rules for compiling .proto files using the prost via the prost protoc plugins."""

load("//:defs.bzl", "proto_compile")

ProstProtoInfo = provider(
    doc = "Additional information needed for prost compilation rules.",
    fields = {
        "crate_name": "Name of the crate that will wrap this module.",
        "declared_proto_packages": "All proto packages that this compile rule generates bindings for.",
    },
)

prost_compile_attrs = [
    "declared_proto_packages",
    "crate_name",
]

def create_name_to_label(name):
    """Convert a simple crate name into its full label."""
    return Label("//rust/3rdparty/crates:" + name)

def prepare_prost_proto_deps(prost_proto_deps):
    """Convert a list of prost proto deps to correct format.

    Args:
        prost_proto_deps: List of target names to be converted.

    Returns:
        List of converted target names.
    """

    # We assume that all targets in prost_proto_deps[] were also generated with this macro.
    # For convenience we append the _pb suffix if its missing to allow users to provide the same
    # name as they used when they used this macro to generate that dependency.
    prost_proto_compiled_targets = []

    for dep in prost_proto_deps:
        if dep.endswith("_pb"):
            prost_proto_compiled_targets.append(dep)
        else:
            prost_proto_compiled_targets.append(dep + "_pb")

    return prost_proto_compiled_targets

def rust_prost_proto_compile_impl(ctx):
    """Implements rust prost proto library.

    Args:
        ctx (Context): The context object passed by bazel.

    Returns:
        list: A list of the following providers:
            - (ProstProtoInfo): Information upstream prost compliation rules need to know about this rule.
            - (ProtoCompileInfo): From core proto_compile function
            - (DefaultInfo): Default build rule info.
    """

    # Build extern options
    externs = []
    for dep in ctx.attr.prost_proto_deps:
        if ProstProtoInfo not in dep:
            continue
        proto_info = dep[ProstProtoInfo]
        packages = proto_info.declared_proto_packages
        dep_crate = proto_info.crate_name or proto_info.name

        for package in packages:
            externs.append("extern_path={}=::{}::{}".format(
                "." + package,
                dep_crate,
                package.replace(".", "::"),
            ))

    options = {}
    for option in ctx.attr.options:
        options[option] = ctx.attr.options[option]
    if "//rust:rust_prost_plugin" not in options:
        options["//rust:rust_prost_plugin"] = []
    options["//rust:rust_prost_plugin"] = options["//rust:rust_prost_plugin"] + externs

    compile_result = proto_compile(
        ctx,
        options,
        getattr(ctx.attr, "extra_protoc_args", []),
        ctx.files.extra_protoc_files,
    )
    crate_name = ctx.attr.crate_name or ctx.attr.name

    return compile_result + [ProstProtoInfo(
        declared_proto_packages = ctx.attr.declared_proto_packages,
        crate_name = crate_name,
    )]
