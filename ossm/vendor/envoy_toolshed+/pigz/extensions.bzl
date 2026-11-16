"""Module extension for prebuilt pigz toolchains."""

load("//:versions.bzl", "VERSIONS")
load(
    "//pigz/private:pigz_prebuilt.bzl",
    "PIGZ_PREBUILT_STRIP_PREFIX",
    "PIGZ_PREBUILT_URL",
    "pigz_prebuilt",
    "pigz_toolchains_hub",
)
load("//private:extension_utils.bzl", "single_setup_tag")

_NO_OVERRIDE = "__envoy_toolshed_pigz_default__"
DEFS_LABEL = str(Label("//pigz:defs.bzl"))
TOOLCHAIN_TYPE_LABEL = str(Label("//pigz:toolchain_type"))

def _pigz_prebuilt_ext_impl(module_ctx):
    setup_tag = single_setup_tag(
        module_ctx = module_ctx,
        ext_name = "pigz_prebuilt_extension",
        repos = "@pigz_prebuilt_linux_x86_64, @pigz_prebuilt_linux_aarch64, @pigz_toolchains",
        attrs = ["host_path", "linux_x86_64_sha256", "linux_aarch64_sha256"],
    )

    platform_labels = {}
    for platform in sorted(VERSIONS["pigz_sha256"]):
        attr_name = platform.replace("-", "_") + "_sha256"
        override = getattr(setup_tag, attr_name) if setup_tag else _NO_OVERRIDE
        sha256 = VERSIONS["pigz_sha256"][platform] if override == _NO_OVERRIDE else override
        if not sha256:
            continue
        repo_name = "pigz_prebuilt_" + platform.replace("-", "_")
        pigz_prebuilt(
            name = repo_name,
            sha256 = sha256,
            strip_prefix = PIGZ_PREBUILT_STRIP_PREFIX.format(
                platform = platform,
                version = VERSIONS["pigz"],
            ),
            url = PIGZ_PREBUILT_URL.format(
                bins_release = VERSIONS["bins_release"],
                platform = platform,
                version = VERSIONS["pigz"],
            ),
        )
        platform_labels[platform] = "@%s//:BUILD.bazel" % repo_name

    pigz_toolchains_hub(
        name = "pigz_toolchains",
        defs_label = DEFS_LABEL,
        host_path = setup_tag.host_path if setup_tag else "",
        linux_aarch64 = platform_labels.get("linux-aarch64"),
        linux_x86_64 = platform_labels.get("linux-x86_64"),
        toolchain_type_label = TOOLCHAIN_TYPE_LABEL,
    )

# setup() controls per-platform prebuilt SHAs: unset uses VERSIONS["pigz_sha256"],
# "" disables that prebuilt repo, and any other value overrides the sha256 for that platform.
# `host_path` adds a toolchain over a pigz on the host, unless a prebuilt for it is enabled.
_setup = tag_class(
    attrs = {
        "host_path": attr.string(
            default = "",
            doc = "Absolute path of a pigz on the host (eg `/usr/bin/pigz`), for a toolchain for the host's OS and CPU, unless there is an enabled prebuilt for it: so it is used where there is no prebuilt, or where the prebuilt is disabled (sha256 \"\"). The binary runs where the action does, so it must run there (eg for remote execution).",
        ),
        "linux_aarch64_sha256": attr.string(
            default = _NO_OVERRIDE,
            doc = "SHA256 for the Linux aarch64 prebuilt pigz artifact: unset uses VERSIONS[\"pigz_sha256\"], \"\" disables the prebuilt repo for this platform, any other value overrides the sha256.",
        ),
        "linux_x86_64_sha256": attr.string(
            default = _NO_OVERRIDE,
            doc = "SHA256 for the Linux x86_64 prebuilt pigz artifact: unset uses VERSIONS[\"pigz_sha256\"], \"\" disables the prebuilt repo for this platform, any other value overrides the sha256.",
        ),
    },
)

pigz_prebuilt_extension = module_extension(
    implementation = _pigz_prebuilt_ext_impl,
    tag_classes = {"setup": _setup},
)
