# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Rules for declaring metadata about a package."""

load(
    "@rules_license//rules:providers.bzl",
    "ExperimentalMetadataInfo",
    "PackageInfo",
)

#
# package_info()
#

def _package_info_impl(ctx):
    provider = PackageInfo(
        # Metadata providers must include a type discriminator. We don't need it
        # to collect the providers, but we do need it to write the JSON. We
        # key on the type field to look up the correct block of code to pull
        # data out and format it. We can't to the lookup on the provider class.
        type = "package_info",
        label = ctx.label,
        package_name = ctx.attr.package_name or ctx.build_file_path.rstrip("/BUILD"),
        package_url = ctx.attr.package_url,
        package_version = ctx.attr.package_version,
        purl = ctx.attr.purl,
    )

    # Experimental alternate design, using a generic 'data' back to hold things
    generic_provider = ExperimentalMetadataInfo(
        type = "package_info_alt",
        label = ctx.label,
        data = {
            "package_name": ctx.attr.package_name or ctx.build_file_path.rstrip("/BUILD"),
            "package_url": ctx.attr.package_url,
            "package_version": ctx.attr.package_version,
            "purl": ctx.attr.purl,
        },
    )
    return [provider, generic_provider]

_package_info = rule(
    implementation = _package_info_impl,
    attrs = {
        "package_name": attr.string(
            doc = "A human readable name identifying this package." +
                  " This may be used to produce an index of OSS packages used by" +
                  " an application.",
        ),
        "package_url": attr.string(
            doc = "The URL this instance of the package was download from." +
                  " This may be used to produce an index of OSS packages used by" +
                  " an application.",
        ),
        "package_version": attr.string(
            doc = "A human readable version string identifying this package." +
                  " This may be used to produce an index of OSS packages used" +
                  " by an application.  It should be a value that" +
                  " increases over time, rather than a commit hash.",
        ),
        "purl": attr.string(
            doc = "A pURL conforming to the spec outlined in" +
                  " https://github.com/package-url/purl-spec. This may be used when" +
                  " generating an SBOM.",
        ),
    },
)

# buildifier: disable=function-docstring-args
def package_info(
        name,
        package_name = None,
        package_url = None,
        package_version = None,
        purl = None,
        **kwargs):
    """Wrapper for package_info rule.

    @wraps(_package_info)

    The purl attribute should be a valid pURL, as defined in the
    [pURL spec](https://github.com/package-url/purl-spec).

    Args:
      name: str target name.
      package_name: str A human readable name identifying this package. This
                    may be used to produce an index of OSS packages used by
                    an application.
      package_url: str The canoncial URL this package distribution was retrieved from.
                       Note that, because of local mirroring, that might not be the
                       physical URL it was retrieved from.
      package_version: str A human readable name identifying version of this package.
      purl: str The canonical pURL by which this package is known.
      kwargs: other args. Most are ignored.
    """
    visibility = kwargs.get("visibility") or ["//visibility:public"]
    _package_info(
        name = name,
        package_name = package_name,
        package_url = package_url,
        package_version = package_version,
        purl = purl,
        applicable_licenses = [],
        visibility = visibility,
        tags = [],
        testonly = 0,
    )
