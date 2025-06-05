"extensions for bzlmod"

load(":repositories.bzl", "DEFAULT_NODE_VERSION", "nodejs_register_toolchains")

def _toolchain_extension(module_ctx):
    registrations = {}
    for mod in module_ctx.modules:
        for toolchain in mod.tags.toolchain:
            if toolchain.name in registrations.keys():
                if toolchain.node_version == registrations[toolchain.name]:
                    # No problem to register a matching toolchain twice
                    continue
                fail("Multiple conflicting toolchains declared for name {} ({} and {})".format(
                    toolchain.name,
                    toolchain.node_version,
                    registrations[toolchain.name],
                ))
            else:
                registrations[toolchain.name] = toolchain.node_version
    for name, node_version in registrations.items():
        nodejs_register_toolchains(
            name = name,
            node_version = node_version,
            register = False,
        )

node = module_extension(
    implementation = _toolchain_extension,
    tag_classes = {
        "toolchain": tag_class(attrs = {
            "name": attr.string(doc = "Base name for generated repositories"),
            "node_version": attr.string(
                doc = "Version of the Node.js interpreter",
                default = DEFAULT_NODE_VERSION,
            ),
        }),
    },
)
