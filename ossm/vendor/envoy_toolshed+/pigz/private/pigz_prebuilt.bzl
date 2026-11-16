"""Repository rules for prebuilt pigz toolchains."""

PIGZ_PREBUILT_URL = "https://github.com/envoyproxy/toolshed/releases/download/bins-v{bins_release}/pigz-{version}-{platform}.tar.zst"
PIGZ_PREBUILT_STRIP_PREFIX = "pigz-{version}-{platform}"

_PLATFORM_TARGETS = {
    "linux-aarch64": {
        "constraint": "@platforms//cpu:aarch64",
        "name": "linux_aarch64",
    },
    "linux-x86_64": {
        "constraint": "@platforms//cpu:x86_64",
        "name": "linux_x86_64",
    },
}

_HOST_OS_CONSTRAINTS = {
    "linux": "@platforms//os:linux",
    "macos": "@platforms//os:macos",
}

_HOST_CPU_CONSTRAINTS = {
    "aarch64": "@platforms//cpu:aarch64",
    "amd64": "@platforms//cpu:x86_64",
    "arm64": "@platforms//cpu:aarch64",
    "x86_64": "@platforms//cpu:x86_64",
}

_HOST_CPUS = {
    "aarch64": "aarch64",
    "amd64": "x86_64",
    "arm64": "aarch64",
    "x86_64": "x86_64",
}

def host_platform(os_name, arch):
    """The host's platform suffix (as the prebuilts', eg `linux-x86_64`).

    Args:
        os_name: `repository_ctx.os.name`, eg `linux` or `mac os x`.
        arch: `repository_ctx.os.arch`, eg `amd64` or `aarch64`.

    Returns:
        The platform suffix, or None for an unsupported OS or CPU.
    """
    os_name = os_name.lower()
    os_key = "macos" if os_name.startswith("mac") else os_name
    cpu = _HOST_CPUS.get(arch.lower())
    if os_key not in _HOST_OS_CONSTRAINTS or not cpu:
        return None
    return "%s-%s" % (os_key, cpu)

def host_constraints(platform):
    """The OS and CPU constraints for a `host_platform` suffix.

    Args:
        platform: Platform suffix from `host_platform`.

    Returns:
        The OS and CPU constraint labels.
    """
    os_key, cpu = platform.split("-", 1)
    return [_HOST_OS_CONSTRAINTS[os_key], _HOST_CPU_CONSTRAINTS[cpu]]

def render_pigz_toolchains_build(platform_repos, defs_label, toolchain_type_label, host = None):
    """Render the hub BUILD file for the configured prebuilt and host toolchains.

    Args:
        platform_repos: Mapping from platform suffix to prebuilt repository name.
        defs_label: Canonical label string for //pigz:defs.bzl.
        toolchain_type_label: Canonical label string for //pigz:toolchain_type.
        host: The host's OS and CPU constraints (`host_constraints`), to render a toolchain over
            the hub's `host/pigz` (a symlink to the configured host pigz), or None. Only for a
            host without an enabled prebuilt, so that the two never match the same platform:
            `@pigz_toolchains//:all` registers in name order, not in the order rendered.

    Returns:
        BUILD file content for the hub repository.
    """
    lines = [
        "load(\"%s\", \"pigz_toolchain\")" % defs_label,
        "",
    ]
    for platform in sorted(_PLATFORM_TARGETS):
        repo_name = platform_repos.get(platform)
        if not repo_name:
            continue
        target = _PLATFORM_TARGETS[platform]
        lines.extend([
            "pigz_toolchain(",
            "    name = \"%s_impl\"," % target["name"],
            # workspace_name is canonical here, so the hub must render @@ labels.
            "    pigz = \"@" + "@%s//:pigz\"," % repo_name,
            ")",
            "",
            "toolchain(",
            "    name = \"%s\"," % target["name"],
            "    toolchain = \":%s_impl\"," % target["name"],
            "    toolchain_type = \"%s\"," % toolchain_type_label,
            # The prebuilt runs on the platform it is resolved for (see `pigz_toolchain`).
            "    target_compatible_with = [",
            "        \"@platforms//os:linux\",",
            "        \"%s\"," % target["constraint"],
            "    ],",
            ")",
            "",
        ])
    if host:
        lines.extend([
            "# The host's pigz (`setup(host_path = ...)`), where there is no prebuilt.",
            "filegroup(",
            "    name = \"host_pigz\",",
            "    srcs = [\"host/pigz\"],",
            ")",
            "",
            "pigz_toolchain(",
            "    name = \"host_impl\",",
            "    pigz = \":host_pigz\",",
            ")",
            "",
            "toolchain(",
            "    name = \"host\",",
            "    toolchain = \":host_impl\",",
            "    toolchain_type = \"%s\"," % toolchain_type_label,
            "    target_compatible_with = [",
        ] + ["        \"%s\"," % constraint for constraint in host] + [
            "    ],",
            ")",
            "",
        ])
    return "\n".join(lines)

def _pigz_prebuilt_impl(ctx):
    if not ctx.attr.sha256:
        fail("pigz_prebuilt requires a non-empty sha256")
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        stripPrefix = ctx.attr.strip_prefix,
    )
    return ctx.repo_metadata(reproducible = True)

pigz_prebuilt = repository_rule(
    implementation = _pigz_prebuilt_impl,
    attrs = {
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "url": attr.string(mandatory = True),
    },
)

def _link_host_pigz(ctx):
    """Symlink the configured host pigz to `host/pigz`, failing unless it is usable.

    Args:
        ctx: Repository context of the hub.

    Returns:
        The host's OS and CPU constraints, for its toolchain, or None if there is an enabled
        prebuilt for the host's platform (which takes precedence).
    """
    path = ctx.attr.host_path
    if not path.startswith("/"):
        fail("pigz: `host_path` must be an absolute path, got %r" % path)
    platform = host_platform(ctx.os.name, ctx.os.arch)
    if not platform:
        fail("pigz: `host_path` is not supported on this host (%s, %s)" % (ctx.os.name, ctx.os.arch))
    if getattr(ctx.attr, platform.replace("-", "_"), None):
        # A prebuilt for the host's platform is enabled, and takes precedence.
        return None
    pigz = ctx.path(path)

    # Re-evaluate the hub if the host pigz changes (or appears, or goes).
    ctx.watch(pigz)
    if not pigz.exists or pigz.is_dir:
        fail("pigz: `host_path` %s does not exist, or is not a file" % path)
    ctx.symlink(pigz, "host/pigz")
    return host_constraints(platform)

def _pigz_toolchains_hub_impl(ctx):
    host = _link_host_pigz(ctx) if ctx.attr.host_path else None
    ctx.file("BUILD.bazel", render_pigz_toolchains_build(
        {
            "linux-aarch64": ctx.attr.linux_aarch64.workspace_name if ctx.attr.linux_aarch64 else None,
            "linux-x86_64": ctx.attr.linux_x86_64.workspace_name if ctx.attr.linux_x86_64 else None,
        },
        ctx.attr.defs_label,
        ctx.attr.toolchain_type_label,
        host = host,
    ))

    # A host pigz is outside the repo's inputs: the hub is only reproducible without one.
    return ctx.repo_metadata(reproducible = not ctx.attr.host_path)

pigz_toolchains_hub = repository_rule(
    implementation = _pigz_toolchains_hub_impl,
    attrs = {
        "defs_label": attr.string(mandatory = True),
        "host_path": attr.string(default = ""),
        "linux_aarch64": attr.label(default = None),
        "linux_x86_64": attr.label(default = None),
        "toolchain_type_label": attr.string(mandatory = True),
    },
)
