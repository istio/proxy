# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
EXPERIMENTAL: This is experimental and may be removed without notice

A module extension for working with uv.
"""

load("//python/private:auth.bzl", "AUTH_ATTRS", "get_auth")
load("//python/private:common_labels.bzl", "labels")
load(":toolchain_types.bzl", "UV_TOOLCHAIN_TYPE")
load(":uv_repository.bzl", "uv_repository")
load(":uv_toolchains_repo.bzl", "uv_toolchains_repo")

_DOC = """\
A module extension for working with uv.

Basic usage:
```starlark
uv = use_extension(
    "@rules_python//python/uv:uv.bzl",
    "uv",
    # Use `dev_dependency` so that the toolchains are not defined pulled when
    # your module is used elsewhere.
    dev_dependency = True,
)
uv.configure(version = "0.5.24")
```

Since this is only for locking the requirements files, it should be always
marked as a `dev_dependency`.
"""

_DEFAULT_ATTRS = {
    "base_url": attr.string(
        doc = """\
Base URL to download metadata about the binaries and the binaries themselves.
""",
    ),
    "compatible_with": attr.label_list(
        doc = """\
The compatible with constraint values for toolchain resolution.
""",
    ),
    "manifest_filename": attr.string(
        doc = """\
The distribution manifest filename to use for the metadata fetching from GH. The
defaults for this are set in `rules_python` MODULE.bazel file that one can override
for a specific version.
""",
        default = "dist-manifest.json",
    ),
    "platform": attr.string(
        doc = """\
The platform string used in the UV repository to denote the platform triple.
""",
    ),
    "target_settings": attr.label_list(
        doc = """\
The `target_settings` to add to platform definitions that then get used in `toolchain`
definitions.
""",
    ),
    "version": attr.string(
        doc = """\
The version of uv to configure the sources for. If this is not specified it will be the
last version used in the module or the default version set by `rules_python`.
""",
    ),
} | AUTH_ATTRS

default = tag_class(
    doc = """\
Set the uv configuration defaults.
""",
    attrs = _DEFAULT_ATTRS,
)

configure = tag_class(
    doc = """\
Build the `uv` toolchain configuration by appending the provided configuration.
The information is appended to the version configuration that is specified by
{attr}`version` attribute, or if the version is unspecified, the version of the
last {obj}`uv.configure` call in the current module, or the version from the
defaults is used.

Complex configuration example:
```starlark
# Configure the base_url for the default version.
uv.configure(base_url = "my_mirror")

# Add an extra platform that can be used with your version.
uv.configure(
    platform = "extra-platform",
    target_settings = ["//my_config_setting_label"],
    compatible_with = ["@platforms//os:exotic"],
)

# Add an extra platform that can be used with your version.
uv.configure(
    platform = "patched-binary",
    target_settings = ["//my_super_config_setting"],
    urls = ["https://example.zip"],
    sha256 = "deadbeef",
)
```
""",
    attrs = _DEFAULT_ATTRS | {
        "sha256": attr.string(
            doc = "The sha256 of the downloaded artifact if the {attr}`urls` is specified.",
        ),
        "urls": attr.string_list(
            doc = """\
The urls to download the binary from. If this is used, {attr}`base_url` and
{attr}`manifest_filename` are ignored for the given version.

::::note
If the `urls` are specified, they need to be specified for all of the platforms
for a particular version.
::::
""",
        ),
    },
)

def _configure(config, *, platform, compatible_with, target_settings, auth_patterns, urls = [], sha256 = "", override = False, **values):
    """Set the value in the config if the value is provided"""
    for key, value in values.items():
        if not value:
            continue

        if not override and config.get(key):
            continue

        config[key] = value

    config.setdefault("auth_patterns", {}).update(auth_patterns)
    config.setdefault("platforms", {})
    if not platform:
        if compatible_with or target_settings or urls:
            fail("`platform` name must be specified when specifying `compatible_with`, `target_settings` or `urls`")
    elif compatible_with or target_settings:
        if not override and config.get("platforms", {}).get(platform):
            return

        config["platforms"][platform] = struct(
            name = platform.replace("-", "_").lower(),
            compatible_with = compatible_with,
            target_settings = target_settings,
        )
    elif urls:
        if not override and config.get("urls", {}).get(platform):
            return

        config.setdefault("urls", {})[platform] = struct(
            sha256 = sha256,
            urls = urls,
        )
    else:
        config["platforms"].pop(platform)

def process_modules(
        module_ctx,
        hub_name = "uv",
        uv_repository = uv_repository,
        toolchain_type = str(UV_TOOLCHAIN_TYPE),
        hub_repo = uv_toolchains_repo,
        get_auth = get_auth):
    """Parse the modules to get the config for 'uv' toolchains.

    Args:
        module_ctx: the context.
        hub_name: the name of the hub repository.
        uv_repository: the rule to create a uv_repository override.
        toolchain_type: the toolchain type to use here.
        hub_repo: the hub repo factory function to use.
        get_auth: the auth function to use.

    Returns:
        the result of the hub_repo. Mainly used for tests.
    """

    # default values to apply for version specific config
    defaults = {
        "base_url": "",
        "manifest_filename": "",
        "platforms": {
            # The structure is as follows:
            # "platform_name": struct(
            #     compatible_with = [],
            #     target_settings = [],
            # ),
            #
            # NOTE: urls and sha256 cannot be set in defaults
        },
        "version": "",
    }
    for mod in module_ctx.modules:
        if not (mod.is_root or mod.name == "rules_python"):
            continue

        for tag in mod.tags.default:
            _configure(
                defaults,
                version = tag.version,
                base_url = tag.base_url,
                manifest_filename = tag.manifest_filename,
                platform = tag.platform,
                compatible_with = tag.compatible_with,
                target_settings = tag.target_settings,
                override = mod.is_root,
                netrc = tag.netrc,
                auth_patterns = tag.auth_patterns,
            )

    for key in [
        "version",
        "manifest_filename",
        "platforms",
    ]:
        if not defaults.get(key, None):
            fail("defaults need to be set for '{}'".format(key))

    # resolved per-version configuration. The shape is something like:
    # versions = {
    #     "1.0.0": {
    #         "base_url": "",
    #         "manifest_filename": "",
    #         "platforms": {
    #             "platform_name": struct(
    #                 compatible_with = [],
    #                 target_settings = [],
    #                 urls = [], # can be unset
    #                 sha256 = "", # can be unset
    #             ),
    #         },
    #     },
    # }
    versions = {}
    for mod in module_ctx.modules:
        if not (mod.is_root or mod.name == "rules_python"):
            continue

        # last_version is the last version used in the MODULE.bazel or the default
        last_version = None
        for tag in mod.tags.configure:
            last_version = tag.version or last_version or defaults["version"]
            specific_config = versions.setdefault(
                last_version,
                {
                    "base_url": defaults["base_url"],
                    "manifest_filename": defaults["manifest_filename"],
                    # shallow copy is enough as the values are structs and will
                    # be replaced on modification
                    "platforms": dict(defaults["platforms"]),
                },
            )

            _configure(
                specific_config,
                base_url = tag.base_url,
                manifest_filename = tag.manifest_filename,
                platform = tag.platform,
                compatible_with = tag.compatible_with,
                target_settings = tag.target_settings,
                sha256 = tag.sha256,
                urls = tag.urls,
                override = mod.is_root,
                netrc = tag.netrc,
                auth_patterns = tag.auth_patterns,
            )

    if not versions:
        return hub_repo(
            name = hub_name,
            toolchain_type = toolchain_type,
            toolchain_names = ["none"],
            toolchain_implementations = {
                # NOTE @aignas 2025-02-24: the label to the toolchain can be anything
                "none": labels.NONE,
            },
            toolchain_compatible_with = {
                "none": ["@platforms//:incompatible"],
            },
            toolchain_target_settings = {},
        )

    toolchain_names = []
    toolchain_implementations = {}
    toolchain_compatible_with_by_toolchain = {}
    toolchain_target_settings = {}
    for version, config in versions.items():
        platforms = config["platforms"]

        # Use the manually specified urls
        urls = {
            platform: src
            for platform, src in config.get("urls", {}).items()
            if src.urls
        }
        auth = {
            "auth_patterns": config.get("auth_patterns"),
            "netrc": config.get("netrc"),
        }
        auth = {k: v for k, v in auth.items() if v}

        # Or fallback to fetching them from GH manifest file
        # Example file: https://github.com/astral-sh/uv/releases/download/0.6.3/dist-manifest.json
        if not urls:
            urls = _get_tool_urls_from_dist_manifest(
                module_ctx,
                base_url = "{base_url}/{version}".format(
                    version = version,
                    base_url = config["base_url"],
                ),
                manifest_filename = config["manifest_filename"],
                platforms = sorted(platforms),
                get_auth = get_auth,
                **auth
            )

        for platform_name, platform in platforms.items():
            if platform_name not in urls:
                continue

            toolchain_name = "{}_{}".format(version.replace(".", "_"), platform_name.lower().replace("-", "_"))
            uv_repository_name = "{}_{}".format(hub_name, toolchain_name)
            uv_repository(
                name = uv_repository_name,
                version = version,
                platform = platform_name,
                urls = urls[platform_name].urls,
                sha256 = urls[platform_name].sha256,
                **auth
            )

            toolchain_names.append(toolchain_name)
            toolchain_implementations[toolchain_name] = "@{}//:uv_toolchain".format(uv_repository_name)
            toolchain_compatible_with_by_toolchain[toolchain_name] = [
                str(label)
                for label in platform.compatible_with
            ]
            if platform.target_settings:
                toolchain_target_settings[toolchain_name] = [
                    str(label)
                    for label in platform.target_settings
                ]

    return hub_repo(
        name = hub_name,
        toolchain_type = toolchain_type,
        toolchain_names = toolchain_names,
        toolchain_implementations = toolchain_implementations,
        toolchain_compatible_with = toolchain_compatible_with_by_toolchain,
        toolchain_target_settings = toolchain_target_settings,
    )

def _uv_toolchain_extension(module_ctx):
    process_modules(
        module_ctx,
        hub_name = "uv",
    )

def _overlap(first_collection, second_collection):
    for x in first_collection:
        if x in second_collection:
            return True

    return False

def _get_tool_urls_from_dist_manifest(module_ctx, *, base_url, manifest_filename, platforms, get_auth = get_auth, **auth_attrs):
    """Download the results about remote tool sources.

    This relies on the tools using the cargo packaging to infer the actual
    sha256 values for each binary.

    Example manifest url: https://github.com/astral-sh/uv/releases/download/0.6.5/dist-manifest.json

    The example format is as below

        dist_version	"0.28.0"
        announcement_tag	"0.6.5"
        announcement_tag_is_implicit	false
        announcement_is_prerelease	false
        announcement_title	"0.6.5"
        announcement_changelog	"text"
        announcement_github_body	"MD text"
        releases	[
            {
                app_name	"uv"
                app_version	"0.6.5"
                env	
                    install_dir_env_var	"UV_INSTALL_DIR"
                    unmanaged_dir_env_var	"UV_UNMANAGED_INSTALL"
                    disable_update_env_var	"UV_DISABLE_UPDATE"
                    no_modify_path_env_var	"UV_NO_MODIFY_PATH"
                    github_base_url_env_var	"UV_INSTALLER_GITHUB_BASE_URL"
                    ghe_base_url_env_var	"UV_INSTALLER_GHE_BASE_URL"
                display_name	"uv"
                display	true
            artifacts	[
                "source.tar.gz"
                "source.tar.gz.sha256"
                "uv-installer.sh"
                "uv-installer.ps1"
                "sha256.sum"
                "uv-aarch64-apple-darwin.tar.gz"
                "uv-aarch64-apple-darwin.tar.gz.sha256"
                "...
            ]
        artifacts	
            uv-aarch64-apple-darwin.tar.gz	
                name	"uv-aarch64-apple-darwin.tar.gz"
                kind	"executable-zip"
                target_triples	[
                    "aarch64-apple-darwin"
                assets	[
                    {
                        id	"uv-aarch64-apple-darwin-exe-uv"
                        name	"uv"
                        path	"uv"
                        kind	"executable"
                    },
                    {
                        id	"uv-aarch64-apple-darwin-exe-uvx"
                        name	"uvx"
                        path	"uvx"
                        kind	"executable"
                    }
                ]
                checksum	"uv-aarch64-apple-darwin.tar.gz.sha256"
            uv-aarch64-apple-darwin.tar.gz.sha256	
                name	"uv-aarch64-apple-darwin.tar.gz.sha256"
                kind	"checksum"
                target_triples	[
                    "aarch64-apple-darwin"
                ]
    """
    auth_attr = struct(**auth_attrs)
    dist_manifest = module_ctx.path(manifest_filename)
    urls = [base_url + "/" + manifest_filename]
    result = module_ctx.download(
        url = urls,
        output = dist_manifest,
        auth = get_auth(module_ctx, urls, ctx_attr = auth_attr),
    )
    if not result.success:
        fail(result)
    dist_manifest = json.decode(module_ctx.read(dist_manifest))

    artifacts = dist_manifest["artifacts"]
    tool_sources = {}
    downloads = {}
    for fname, artifact in artifacts.items():
        if artifact.get("kind") != "executable-zip":
            continue

        checksum = artifacts[artifact["checksum"]]
        if not _overlap(checksum["target_triples"], platforms):
            # we are not interested in this platform, so skip
            continue

        checksum_fname = checksum["name"]
        checksum_path = module_ctx.path(checksum_fname)
        urls = ["{}/{}".format(base_url, checksum_fname)]
        downloads[checksum_path] = struct(
            download = module_ctx.download(
                url = urls,
                output = checksum_path,
                block = False,
                auth = get_auth(module_ctx, urls, ctx_attr = auth_attr),
            ),
            archive_fname = fname,
            platforms = checksum["target_triples"],
        )

    for checksum_path, download in downloads.items():
        result = download.download.wait()
        if not result.success:
            fail(result)

        archive_fname = download.archive_fname

        sha256, _, checksummed_fname = module_ctx.read(checksum_path).partition(" ")
        checksummed_fname = checksummed_fname.strip(" *\n")
        if checksummed_fname and archive_fname != checksummed_fname:
            fail("The checksum is for a different file, expected '{}' but got '{}'".format(
                archive_fname,
                checksummed_fname,
            ))

        for platform in download.platforms:
            tool_sources[platform] = struct(
                urls = ["{}/{}".format(base_url, archive_fname)],
                sha256 = sha256,
            )

    return tool_sources

uv = module_extension(
    doc = _DOC,
    implementation = _uv_toolchain_extension,
    tag_classes = {
        "configure": configure,
        "default": default,
    },
)
