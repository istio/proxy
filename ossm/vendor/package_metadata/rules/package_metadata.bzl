"""Declares rule `package_metadata`."""

load("//providers:package_attribute_info.bzl", "PackageAttributeInfo")
load("//providers:package_metadata_info.bzl", "PackageMetadataInfo")

visibility("public")

def _package_metadata_impl(ctx):
    attributes = [a[PackageAttributeInfo] for a in ctx.attr.attributes]

    metadata = ctx.actions.declare_file("{}.package-metadata.json".format(ctx.attr.name))

    ctx.actions.write(
        output = metadata,
        content = json.encode({
            "attributes": {a.kind: a.attributes.path for a in attributes},
            "label": str(ctx.label),
            "purl": ctx.attr.purl,
        }),
    )

    return [
        DefaultInfo(
            files = depset(
                direct = [
                    metadata,
                ],
            ),
        ),
        PackageMetadataInfo(
            metadata = metadata,
            files = [a.files for a in attributes],
        ),
    ]

_package_metadata = rule(
    implementation = _package_metadata_impl,
    attrs = {
        "attributes": attr.label_list(
            mandatory = False,
            doc = """
A list of `attributes` of the package (e.g., source location, license, ...).
""".strip(),
            providers = [
                PackageAttributeInfo,
            ],
        ),
        "purl": attr.string(
            mandatory = True,
            doc = """
The [PURL](https://github.com/package-url/purl-spec) uniquely identifying this
package.
""".strip(),
        ),
    },
    provides = [
        PackageMetadataInfo,
    ],
    doc = """
Rule for declaring `PackageMetadataInfo`, typically of a `bzlmod` module.
""".strip(),
)

def package_metadata(
        # Disallow unnamed attributes.
        *,
        # `_package_metadata` attributes.
        name,
        purl,
        attributes = [],
        # Common attributes (subset since this target is non-configurable).
        visibility = None):
    _package_metadata(
        # `_package_metadata` attributes.
        name = name,
        purl = purl,
        attributes = attributes,

        # Common attributes.
        visibility = visibility,
        applicable_licenses = [],
    )
