"extensions for bzlmod"

load(":pull.bzl", "oci_pull")
load(":repositories.bzl", "oci_register_toolchains")

# TODO: it sucks that the API of the oci_pull macro has to be repeated here.
pull = tag_class(attrs = {
    "name": attr.string(doc = "Name of the generated repository"),
    "www_authenticate_challenges": attr.string_dict(doc = """a dictionary of registry hostnames to WWW-Authenticate challenges.
        This is used to perform authentication challenges for private registries.
        See the [documentation](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/WWW-Authenticate)
        for more information about the WWW-Authenticate header.
    """),
    "registry": attr.string(doc = """the remote registry domain, such as `gcr.io` or `docker.io`.
            When set, `repository` must also be set, and `image` must not be set."""),
    "repository": attr.string(doc = """the image path beneath the registry, such as `distroless/static`.
            When set, `registry` must also be set, and `image` must not be set."""),
    "image": attr.string(doc = """the remote image without a tag, such as `gcr.io/bazel-public/bazel`.
            When set, `registry` and `repository` must not be set."""),
    "platforms": attr.string_list(doc = """for multi-architecture images, a dictionary of the platforms it supports
            This creates a separate external repository for each platform, avoiding fetching layers."""),
    "digest": attr.string(doc = """the digest string, starting with "sha256:", "sha512:", etc.
            If omitted, instructions for pinning are provided."""),
    "tag": attr.string(doc = """a tag to choose an image from the registry.
            Exactly one of `tag` and `digest` must be set.
            Since tags are mutable, this is not reproducible, so a warning is printed."""),
    "reproducible": attr.bool(doc = """Set to False to silence the warning about reproducibility when using `tag`.""", default = True),
    "config": attr.label(doc = "Label to a .docker/config.json file"),
    "bazel_tags": attr.string_list(doc = """Bazel tags to be propagated to generated rules."""),
})

toolchains = tag_class(attrs = {
    "name": attr.string(doc = """\
Base name for generated repositories, allowing more than one set of toolchains to be registered.
Overriding the default is only permitted in the root module.
""", default = "oci"),
})

def _oci_extension(module_ctx):
    root_direct_deps = []
    root_direct_dev_deps = []
    reproducible = True
    for mod in module_ctx.modules:
        for pull in mod.tags.pull:
            if not pull.reproducible or not pull.digest:
                reproducible = False

            oci_pull(
                name = pull.name,
                image = pull.image,
                registry = pull.registry,
                repository = pull.repository,
                platforms = pull.platforms,
                digest = pull.digest,
                tag = pull.tag,
                reproducible = pull.reproducible,
                config = pull.config,
                bazel_tags = pull.bazel_tags,
                www_authenticate_challenges = pull.www_authenticate_challenges,
                is_bzlmod = True,
            )

            if mod.is_root:
                deps = root_direct_dev_deps if module_ctx.is_dev_dependency(pull) else root_direct_deps
                deps.append(pull.name)
                for platform in pull.platforms:
                    deps.append("_".join([pull.name] + platform.split("/")))

        for toolchains in mod.tags.toolchains:
            if toolchains.name != "oci" and not mod.is_root:
                fail("""\
                Only the root module may override the default name for the oci toolchains.
                This prevents conflicting registrations in the global namespace of external repos.
                """)
            if mod.is_root:
                deps = root_direct_dev_deps if module_ctx.is_dev_dependency(toolchains) else root_direct_deps
                deps.append("%s_crane_toolchains" % toolchains.name)
                deps.append("%s_regctl_toolchains" % toolchains.name)

            oci_register_toolchains(toolchains.name, register = False)

    # Allow use_repo calls to be automatically managed by `bazel mod tidy`. See
    # https://docs.google.com/document/d/1dj8SN5L6nwhNOufNqjBhYkk5f-BJI_FPYWKxlB3GAmA/edit#heading=h.5mcn15i0e1ch
    return module_ctx.extension_metadata(
        root_module_direct_deps = root_direct_deps,
        root_module_direct_dev_deps = root_direct_dev_deps,
        reproducible = reproducible,
    )

oci = module_extension(
    implementation = _oci_extension,
    tag_classes = {
        "pull": pull,
        "toolchains": toolchains,
    },
)
