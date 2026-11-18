"Implementation details for oci_pull repository rules"

load("@aspect_bazel_lib//lib:copy_to_directory.bzl", _copy_to_directory = "copy_to_directory")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:versions.bzl", "versions")
load("@package_metadata//rules:package_metadata.bzl", _package_metadata = "package_metadata")
load("//oci/private:authn.bzl", "authn")
load("//oci/private:util.bzl", "util")

# Reexports for bzlmod resolution in generated `oci_pull` repositories.
copy_to_directory = _copy_to_directory
package_metadata = _package_metadata

# attributes that are specific to image reference url. shared between multiple targets
_IMAGE_REFERENCE_ATTRS = {
    "scheme": attr.string(
        doc = "scheme portion of the URL for fetching from the registry",
        values = ["http", "https"],
        default = "https",
    ),
    "registry": attr.string(
        doc = "Remote registry host to pull from, e.g. `gcr.io` or `index.docker.io`",
        mandatory = True,
    ),
    "repository": attr.string(
        doc = "Image path beneath the registry, e.g. `distroless/static`",
        mandatory = True,
    ),
    "identifier": attr.string(
        doc = "The digest or tag of the manifest file",
        mandatory = True,
    ),
    "config": attr.label(
        doc = "Label to a .docker/config.json file",
        allow_single_file = True,
    ),
    "www_authenticate_challenges": attr.string_dict(
        doc = "EXPERIMENTAL! Additional WWW-Authenticate entries for private registries.",
        default = {},
    ),
}

SCHEMA1_ERROR = """\
The registry sent a manifest with schemaVersion=1.
This commonly occurs when fetching from a registry that needs the Docker-Distribution-API-Version header to be set.
See: https://github.com/bazel-contrib/rules_oci/blob/main/docs/pull.md#authentication-using-credential-helpers
"""

OCI_MEDIA_TYPE_OR_AUTHN_ERROR = """\
Unable to retrieve the image manifest. This could be due to
*) Authentication problems. Check if `docker pull` command succeeds with same parameters.
*) Fetching an image with OCI image media types.
*) If there is a configured URL Rewriter, check that it does not block the request.

See for more: https://github.com/bazel-contrib/rules_oci/blob/main/docs/pull.md#authentication-using-credential-helpers
"""

OCI_MEDIA_TYPE_OR_AUTHN_ERROR_BAZEL7 = """\
Unable to retrieve the image manifest. This could be due to
*) Authentication problems. Check if `docker pull` command succeeds with same parameters.
*) If there is a configured URL Rewriter, check that it does not block the request.
"""

_DEFAULT_MANIFEST_MEDIA_TYPE = "application/vnd.oci.image.manifest.v1+json"
_DEFAULT_INDEX_MEDIA_TYPE = "application/vnd.oci.image.index.v1+json"

# Supported media types
# * OCI spec: https://github.com/opencontainers/image-spec/blob/main/media-types.md
# * Docker spec: https://github.com/distribution/distribution/blob/main/docs/spec/manifest-v2-2.md#media-types
_SUPPORTED_MEDIA_TYPES = {
    "index": [
        "application/vnd.docker.distribution.manifest.list.v2+json",
        _DEFAULT_INDEX_MEDIA_TYPE,
    ],
    "manifest": [
        "application/vnd.docker.distribution.manifest.v2+json",
        _DEFAULT_MANIFEST_MEDIA_TYPE,
    ],
}

_DOWNLOAD_HEADERS = {
    "Accept": ",".join(_SUPPORTED_MEDIA_TYPES["index"] + _SUPPORTED_MEDIA_TYPES["manifest"]),
    "Docker-Distribution-API-Version": "registry/2.0",
}

def _determine_media_type(manifest):
    """Determine the media type of a manifest.

    In the absence of an explicit media type, infer a default value based on other properties.
    Image manifests have a required `config` field:
    https://specs.opencontainers.org/image-spec/manifest/#image-manifest-property-descriptions.
    Image indices have a required `manifests` field:
    https://specs.opencontainers.org/image-spec/image-index/#image-index-property-descriptions
    """
    if "mediaType" in manifest:
        return manifest["mediaType"]
    if "config" in manifest:
        return _DEFAULT_MANIFEST_MEDIA_TYPE
    if "manifests" in manifest:
        return _DEFAULT_INDEX_MEDIA_TYPE
    fail("Unable to determine media type")

def _config_path(rctx):
    if rctx.attr.config:
        return rctx.path(rctx.attr.config)
    return None

def _is_tag(str):
    return str.find(":") == -1

# The only digest algorithm `_download` hands to the Bazel downloader as an integrity check.
# `oci_pull` documents `sha512:` and friends as valid too, but those are fetched unverified.
_VERIFIED_DIGEST_PREFIX = "sha256:"

def _is_verified_digest(identifier):
    """Whether `identifier` pins content that the downloader actually verifies.

    Stricter than `not _is_tag(identifier)`: a digest in an algorithm we do not check is no more
    trustworthy than a tag, since nothing detects a registry serving different bytes.
    """
    return identifier.startswith(_VERIFIED_DIGEST_PREFIX)

def _digest_into_blob_path(digest):
    "convert sha256:deadbeef into sha256/deadbeef"
    digest_path = digest.replace(":", "/", 1)
    return "blobs/{}".format(digest_path)

def _download(rctx, authn, identifier, output, resource, headers = {}, allow_fail = False, block = True):
    "Use the Bazel Downloader to fetch from the remote registry"

    if resource != "blobs" and resource != "manifests":
        fail("resource must be blobs or manifests")

    auth = authn.get_token(rctx.attr.registry, rctx.attr.repository)

    # Construct the URL to fetch from remote, see
    # https://github.com/google/go-containerregistry/blob/62f183e54939eabb8e80ad3dbc787d7e68e68a43/pkg/v1/remote/descriptor.go#L234
    registry_url = "{scheme}://{registry}/v2/{repository}/{resource}/{identifier}".format(
        scheme = rctx.attr.scheme,
        registry = rctx.attr.registry,
        repository = rctx.attr.repository,
        resource = resource,
        identifier = identifier,
    )

    sha256 = ""

    if _is_verified_digest(identifier):
        sha256 = identifier[len(_VERIFIED_DIGEST_PREFIX):]
    else:
        util.warning(rctx, "Fetching from {}@{} without an integrity hash, result will not be cached.".format(rctx.attr.repository, identifier))

    kwargs = dict(
        output = output,
        sha256 = sha256,
        url = registry_url,
        auth = {registry_url: auth},
        allow_fail = allow_fail,
    )

    # Use non-blocking download, and forward headers, on Bazel 7.1.0 and later.
    if versions.is_at_least("7.1.0", versions.get()):
        kwargs["block"] = block
        kwargs["headers"] = headers

    return rctx.download(**kwargs)

def _download_manifest(rctx, authn, identifier, output):
    bytes = None
    manifest = None
    digest = None

    result = _download(
        rctx,
        authn,
        identifier,
        output,
        "manifests",
        allow_fail = True,
        headers = _DOWNLOAD_HEADERS,
    )

    if result.success:
        bytes = rctx.read(output)
        manifest = json.decode(bytes)
        digest = "sha256:{}".format(result.sha256)
        if manifest["schemaVersion"] == 1:
            fail(SCHEMA1_ERROR)
    else:
        explanation = authn.explain()
        if explanation:
            util.warning(rctx, explanation)
        fail(
            OCI_MEDIA_TYPE_OR_AUTHN_ERROR_BAZEL7 if versions.is_at_least("7.1.0", versions.get()) else OCI_MEDIA_TYPE_OR_AUTHN_ERROR,
        )

    return manifest, len(bytes), digest

def _create_downloader(rctx, authn):
    return struct(
        download_blob = lambda identifier, output, block: _download(rctx, authn, identifier, output, "blobs", block = block),
        download_manifest = lambda identifier, output: _download_manifest(rctx, authn, identifier, output),
    )

_BUILD_FILE_TMPL = """\
"Generated by oci_pull. DO NOT EDIT!"

load("@rules_oci//oci/private:pull.bzl", "copy_to_directory", "package_metadata")

package_metadata(
    name = "package_metadata",
    purl = {purl},
    visibility = ["//:__subpackages__"],
)

copy_to_directory(
    name = "{target_name}",
    out = "layout",
    # Always use hardlink to avoid having copies of the blobs from external repository in the output-tree.
    hardlink = "on",
    include_external_repositories = ["*"],
    srcs = glob(["blobs/**"]) + [
        "oci-layout",
        "index.json",
    ],
    tags = {bazel_tags},
    visibility = ["//visibility:public"]
)
"""

def _find_platform_manifest(index_mf, platform_wanted):
    """From an index manifest, get the image manifest that corresponds to the given platform"""
    for mf in index_mf["manifests"]:
        parts = [
            mf["platform"]["os"],
            mf["platform"]["architecture"],
        ]
        if "variant" in mf["platform"]:
            parts.append(mf["platform"]["variant"])

        platform = "/".join(parts)
        if platform_wanted == platform:
            return mf
    return None

def _oci_pull_impl(rctx):
    # Pulling by a verified digest is reproducible: every downloaded blob is checked against a
    # digest derived from the `identifier` attribute. A tag is not, since the registry is free to
    # move it to different content at any time, and neither is a digest in an algorithm we cannot
    # check - in both cases nothing would notice the registry serving different bytes.
    reproducible = _is_verified_digest(rctx.attr.identifier)

    au = authn.new(rctx, _config_path(rctx), reproducible = reproducible)
    downloader = _create_downloader(rctx, au)

    manifest, size, digest = downloader.download_manifest(rctx.attr.identifier, "manifest.json")
    media_type = _determine_media_type(manifest)

    matching_manifest = None

    if media_type in _SUPPORTED_MEDIA_TYPES["manifest"]:
        # copy manifest.json to blobs with its digest.
        rctx.template(_digest_into_blob_path(digest), "manifest.json")

    elif media_type in _SUPPORTED_MEDIA_TYPES["index"]:
        # image index manifest: download the image manifest for the target platform.
        if not rctx.attr.platform:
            fail("{}/{} is a multi-architecture image, so attribute 'platforms' is required.".format(
                rctx.attr.registry,
                rctx.attr.repository,
            ))

        matching_manifest = _find_platform_manifest(manifest, rctx.attr.platform)
        if not matching_manifest:
            fail("No matching manifest found in image {}/{} for platform {}".format(
                rctx.attr.registry,
                rctx.attr.repository,
                rctx.attr.platform,
            ))

        # NB: do _NOT_ download to `manifest.json` as there is a race condition in Bazel when it is writing the cache entry
        # for the `manifest.json` above where it may do so only after this download completes and overwrites `manifest.json`.
        # If this race condition occurs, this results in Bazel writing the contents of this download to the cache for the
        # download above which corrupts the cache and causes build failures for other platforms:
        # ```
        # (01:43:58) ERROR: An error occurred during the fetch of repository 'debian_golden_linux_arm64_v8':
        #    Traceback (most recent call last):
        # 	File "/mnt/ephemeral/output/__main__/external/rules_oci/oci/private/pull.bzl", line 241, column 37, in _oci_pull_impl
        # 		util.validate_image_platform(rctx, config)
        # 	File "/mnt/ephemeral/output/__main__/external/rules_oci/oci/private/util.bzl", line 96, column 13, in _validate_image_platform
        # 		fail("Expected image {}/{} to have architecture '{}', got: '{}'".format(
        # Error in fail: Expected image index.docker.io/library/debian to have architecture 'arm64', got: 'amd64'
        # (01:43:58) ERROR: /mnt/ephemeral/workdir/aspect-build/silo/WORKSPACE:305:19: fetching oci_pull rule //external:debian_golden_linux_arm64_v8: Traceback (most recent call last):
        # 	File "/mnt/ephemeral/output/__main__/external/rules_oci/oci/private/pull.bzl", line 241, column 37, in _oci_pull_impl
        # 		util.validate_image_platform(rctx, config)
        # 	File "/mnt/ephemeral/output/__main__/external/rules_oci/oci/private/util.bzl", line 96, column 13, in _validate_image_platform
        # 		fail("Expected image {}/{} to have architecture '{}', got: '{}'".format(
        # Error in fail: Expected image index.docker.io/library/debian to have architecture 'arm64', got: 'amd64'
        # (01:43:58) ERROR: no such package '@@debian_golden_linux_arm64_v8//': Expected image index.docker.io/library/debian to have architecture 'arm64', got: 'amd64'
        # (01:43:58) ERROR: /mnt/ephemeral/output/__main__/external/debian_golden/BUILD.bazel:1:6: @@debian_golden//:debian_golden depends on @@debian_golden_linux_arm64_v8//:debian_golden_linux_arm64_v8 in repository @@debian_golden_linux_arm64_v8 which failed to fetch. no such package '@@debian_golden_linux_arm64_v8//': Expected image index.docker.io/library/debian to have architecture 'arm64', got: 'amd64'
        # ```
        # See https://github.com/bazel-contrib/rules_oci/pull/596 for more details on this race codition.
        manifest, size, digest = downloader.download_manifest(matching_manifest["digest"], "platform-manifest.json")
        media_type = _determine_media_type(manifest)

        # copy platform-manifest.json to blobs with its digest.
        rctx.template(_digest_into_blob_path(digest), "platform-manifest.json")
    else:
        fail("Unrecognized mediaType {} in manifest file".format(media_type))

    config_output_path = _digest_into_blob_path(manifest["config"]["digest"])
    downloader.download_blob(manifest["config"]["digest"], config_output_path, block = True)

    # if the user provided a platform for the image, validate it matches the config as best effort.
    if rctx.attr.platform:
        config_bytes = rctx.read(config_output_path)
        config = json.decode(config_bytes)
        util.validate_image_platform(rctx, config)

    # download all layers
    # TODO: we should avoid eager-download of the layers ("shallow pull")
    results = {}
    for layer in manifest["layers"]:
        # Skip downloading of duplicate layers.
        if layer["digest"] in results:
            continue
        results[layer["digest"]] = downloader.download_blob(
            layer["digest"],
            _digest_into_blob_path(layer["digest"]),
            block = False,
        )

    # wait for all downloads to complete, if download is asynchronous
    for r in results.values():
        if hasattr(r, "wait"):
            r.wait()

    artifact_type = matching_manifest.get("artifactType", None) if matching_manifest else manifest.get("artifactType", None)

    rctx.file("index.json", util.build_manifest_json(
        media_type = media_type,
        size = size,
        digest = digest,
        platform = rctx.attr.platform,
        artifact_type = artifact_type,
    ))
    rctx.file("oci-layout", json.encode_indent({"imageLayoutVersion": "1.0.0"}, indent = "    "))

    bazel_tags = "[\"{}\"]".format("\", \"".join(rctx.attr.bazel_tags))

    # https://github.com/package-url/purl-spec/blob/main/types-doc/docker-definition.md
    purl = "pkg:docker/{}@{}".format(rctx.attr.repository, rctx.attr.identifier)
    if rctx.attr.registry != "":
        purl += "?repository_url=" + rctx.attr.registry

    rctx.file("BUILD.bazel", content = _BUILD_FILE_TMPL.format(
        target_name = rctx.attr.target_name,
        bazel_tags = bazel_tags,
        purl = repr(purl),
    ))

    rctx.file("REPO.bazel", """\
repo(
    default_package_metadata = [
        "//:package_metadata",
    ],
)
""")

    return util.repo_metadata(rctx, reproducible = reproducible)

oci_pull = repository_rule(
    implementation = _oci_pull_impl,
    attrs = dicts.add(
        _IMAGE_REFERENCE_ATTRS,
        {
            "platform": attr.string(
                doc = "A single platform in `os/arch` format, for multi-arch images",
            ),
            "target_name": attr.string(
                doc = "Name given for the image target, e.g. 'image'",
                mandatory = True,
            ),
            "bazel_tags": attr.string_list(
                doc = "Bazel tags to apply to generated targets of this rule",
            ),
        },
    ),
    environ = authn.ENVIRON,
)

_MULTI_PLATFORM_IMAGE_ALIAS_TMPL = """\
filegroup(
    name = "digest",
    srcs = ["digest.txt"],
    visibility = ["//visibility:public"],
)

alias(
    name = "{target_name}",
    actual = select(
        {platform_map},
        no_match_error = \"\"\"could not find an image matching the target platform. \\nAvailable platforms are {available_platforms} \"\"\",
    ),
    visibility = ["//visibility:public"],
)
"""

_SINGLE_PLATFORM_IMAGE_ALIAS_TMPL = """\
filegroup(
    name = "digest",
    srcs = ["digest.txt"],
    visibility = ["//visibility:public"],
)

alias(
    name = "{target_name}",
    actual = "@{original}//:{original}",
    visibility = ["//visibility:public"],
)
"""

def _oci_alias_impl(rctx):
    if rctx.attr.platforms and rctx.attr.platform:
        fail("Only one of 'platforms' or 'platform' may be set")
    if not rctx.attr.platforms and not rctx.attr.platform:
        fail("One of 'platforms' or 'platform' must be set")

    # Same as oci_pull: only a verified digest pins the content this repo describes. Note the
    # generated `digest.txt` is derived from `identifier`, so with a tag it can change between
    # fetches.
    reproducible = _is_verified_digest(rctx.attr.identifier)

    au = authn.new(rctx, _config_path(rctx), reproducible = reproducible)
    downloader = _create_downloader(rctx, au)

    available_platforms = []

    manifest, _, digest = downloader.download_manifest(rctx.attr.identifier, "mf.json")
    media_type = _determine_media_type(manifest)
    rctx.file("digest.txt", digest)

    if rctx.attr.platforms:
        if media_type in _SUPPORTED_MEDIA_TYPES["index"]:
            # multi arch image.
            for submanifest in manifest["manifests"]:
                parts = [submanifest["platform"]["os"], submanifest["platform"]["architecture"]]
                if "variant" in submanifest["platform"]:
                    parts.append(submanifest["platform"]["variant"])
                available_platforms.append('"{}"'.format("/".join(parts)))
        elif media_type in _SUPPORTED_MEDIA_TYPES["manifest"]:
            # single arch image where the user specified the platform.
            config_output_path = _digest_into_blob_path(manifest["config"]["digest"])
            downloader.download_blob(manifest["config"]["digest"], config_output_path, block = True)
            config_bytes = rctx.read(config_output_path)
            config = json.decode(config_bytes)
            if "os" in config and "architecture" in config:
                parts = [config["os"], config["architecture"]]
                if "variant" in config:
                    parts.append(config["variant"])
                available_platforms.append('"{}"'.format("/".join(parts)))
            else:
                available_platforms.append("unknown (os/architecture unspecified in image metadata)")

    if _is_tag(rctx.attr.identifier) and rctx.attr.reproducible:
        is_bzlmod = hasattr(rctx.attr, "bzlmod_repository") and rctx.attr.bzlmod_repository

        optional_platforms = ""

        if len(available_platforms):
            optional_platforms = "'add platforms {}'".format(" ".join(available_platforms))

        util.warning(rctx, """\
For reproducible builds, a digest is recommended.
Either set 'reproducible = False' to silence this warning, or run the following command to change {rule} to use a digest:
{warning}

buildozer 'set digest "{digest}"' 'remove tag' 'remove platforms' {optional_platforms} {location}
    """.format(
            location = "MODULE.bazel:" + rctx.attr.bzlmod_repository if is_bzlmod else "WORKSPACE:" + rctx.attr.name,
            digest = digest,
            optional_platforms = optional_platforms,
            warning = "(make sure you use a recent buildozer release with MODULE.bazel support)" if is_bzlmod else "",
            rule = "oci.pull" if is_bzlmod else "oci_pull",
        ))

    build = ""
    if rctx.attr.platforms:
        build = _MULTI_PLATFORM_IMAGE_ALIAS_TMPL.format(
            name = rctx.attr.name,
            target_name = rctx.attr.target_name,
            available_platforms = ", ".join(available_platforms),
            platform_map = {
                str(k): v
                for k, v in rctx.attr.platforms.items()
            },
        )
    else:
        build = _SINGLE_PLATFORM_IMAGE_ALIAS_TMPL.format(
            name = rctx.attr.name,
            target_name = rctx.attr.target_name,
            original = rctx.attr.platform.name,
        )

    rctx.file("BUILD.bazel", content = build)

    return util.repo_metadata(rctx, reproducible = reproducible)

oci_alias = repository_rule(
    implementation = _oci_alias_impl,
    attrs = dicts.add(
        _IMAGE_REFERENCE_ATTRS,
        {
            "platforms": attr.label_keyed_string_dict(
                doc = "If set, the alias will map the target platform's cpu to the corresponding image, or fail if no image matches.",
            ),
            "platform": attr.label(
                doc = "If set, the alias will simply map to that (single) image, regardless of the target platform's cpu.",
            ),
            "target_name": attr.string(),
            "reproducible": attr.bool(
                default = True,
                doc = "Set to False to silence the warning about reproducibility when using `tag`",
            ),
            "bzlmod_repository": attr.string(
                doc = "For error reporting. When called from a module extension, provides the original name of the repository prior to mapping",
            ),
        },
    ),
    environ = authn.ENVIRON,
)
