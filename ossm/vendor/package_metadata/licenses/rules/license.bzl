"""Declares rule `license`."""

load("//licenses/providers:license_kind_info.bzl", "LicenseKindInfo")
load("//providers:package_attribute_info.bzl", "PackageAttributeInfo")

visibility("public")

def _license_impl(ctx):
    kind = ctx.attr.kind[LicenseKindInfo]
    attribute = {
        "kind": {
            "identifier": kind.identifier,
            "name": kind.name,
        },
        "label": str(ctx.label),
    }
    files = []

    if ctx.attr.text:
        attribute["text"] = ctx.file.text.path
        files.append(ctx.attr.text[DefaultInfo].files)

    output = ctx.actions.declare_file("{}.package-attribute.json".format(ctx.attr.name))
    ctx.actions.write(
        output = output,
        content = json.encode(attribute),
    )

    return [
        DefaultInfo(
            files = depset(
                direct = [
                    output,
                ],
            ),
        ),
        PackageAttributeInfo(
            kind = "build.bazel.attribute.license",
            attributes = output,
            files = files,
        ),
    ]

_license = rule(
    implementation = _license_impl,
    attrs = {
        "kind": attr.label(
            mandatory = True,
            doc = """
The kind of license this license is classified as.

This is typically a `license_kind` target.

See `@package_metadata//licenses/spdx`.
""".strip(),
            providers = [
                LicenseKindInfo,
            ],
        ),
        "text": attr.label(
            mandatory = False,
            allow_single_file = True,
            doc = """
The [File](https://bazel.build/rules/lib/builtins/File) with the text of the
license.
""".strip(),
        ),
    },
    doc = """
Rule for declaring the license of a package or target.

This is typically passed to `package_metadata.attributes`.

Usage:

```starlark
load("@package_metadata//licenses/rules:license.bzl", "license")
load("@package_metadata//rules:package_metadata.bzl", "package_metadata")

package_metadata(
    name = "package_metadata",
    purl = "...",
    attributes = [
        ":license",
    ],
    visibility = ["//visibility:public"],
)

license(
    name = "license",
    kind = "@package_metadata//licenses/spdx:GPL-3.0",
    text = "COPYING",
)
""".strip(),
)

def license(
        # Disallow unnamed attributes.
        *,
        # `_license` attributes.
        name,
        kind,
        text = None,
        # Common attributes (subset since this target is non-configurable).
        visibility = None):
    _license(
        # `_license` attributes.
        name = name,
        kind = kind,
        text = text,

        # Common attributes.
        visibility = visibility,
        applicable_licenses = [],
    )
