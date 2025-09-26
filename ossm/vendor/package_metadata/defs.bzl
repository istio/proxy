"""Public API of `@package_metadata`."""

load("//providers:package_attribute_info.bzl", _PackageAttributeInfo = "PackageAttributeInfo")
load("//providers:package_metadata_info.bzl", _PackageMetadataInfo = "PackageMetadataInfo")
load("//purl:purl.bzl", _purl = "purl")
load("//rules:package_metadata.bzl", _package_metadata = "package_metadata")

visibility("public")

# Providers.
PackageAttributeInfo = _PackageAttributeInfo
PackageMetadataInfo = _PackageMetadataInfo

# Rules
package_metadata = _package_metadata

# Utils
purl = _purl
