"""Declares rule `license_kind`."""

load("//licenses/providers:license_kind_info.bzl", "LicenseKindInfo")

visibility("public")

def _license_kind_impl(ctx):
    return [
        LicenseKindInfo(
            identifier = ctx.attr.identifier,
            name = ctx.attr.full_name,
        ),
    ]

_license_kind = rule(
    implementation = _license_kind_impl,
    attrs = {
        "full_name": attr.string(
            mandatory = True,
            doc = """
The [PURL](https://github.com/package-url/purl-spec) uniquely identifying this
package.
""".strip(),
        ),
        "identifier": attr.string(
            mandatory = True,
            doc = """
The unique identifier of the license (e.g., `Apache-2.0`, `EUPL-1.1`).

This is typically the [SPDX identifier](https://spdx.org/licenses/) of the
license, but may also be a non-standard value (e.g., in case of a commercial
license).
""".strip(),
        ),
    },
    provides = [
        LicenseKindInfo,
    ],
    doc = """
Rule for declaring `LicenseKindInfo`.
""".strip(),
)

def license_kind(
        # Disallow unnamed attributes.
        *,
        # `_license_kind` attributes.
        name,
        identifier,
        full_name,
        # Common attributes (subset since this target is non-configurable).
        visibility = None):
    _license_kind(
        # `_license_kind` attributes.
        name = name,
        identifier = identifier,
        full_name = full_name,

        # Common attributes.
        visibility = visibility,
        applicable_licenses = [],
    )
