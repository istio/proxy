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
"""License compliance checking."""

load(
    "@rules_license//rules:gather_licenses_info.bzl",
    "gather_licenses_info",
    "write_licenses_info",
)

def _licenses_used_impl(ctx):
    # Gather all licenses and make it available as JSON
    write_licenses_info(ctx, ctx.attr.deps, ctx.outputs.out)
    return [DefaultInfo(files = depset([ctx.outputs.out]))]

_licenses_used = rule(
    implementation = _licenses_used_impl,
    doc = """Internal tmplementation method for licenses_used().""",
    attrs = {
        "deps": attr.label_list(
            doc = """List of targets to collect LicenseInfo for.""",
            aspects = [gather_licenses_info],
        ),
        "out": attr.output(
            doc = """Output file.""",
            mandatory = True,
        ),
    },
)

def licenses_used(name, deps, out = None, **kwargs):
    """Collects LicensedInfo providers for a set of targets and writes as JSON.

    The output is a single JSON array, with an entry for each license used.
    See gather_licenses_info.bzl:write_licenses_info() for a description of the schema.

    Args:
      name: The target.
      deps: A list of targets to get LicenseInfo for. The output is the union of
            the result, not a list of information for each dependency.
      out: The output file name. Default: <name>.json.
      **kwargs: Other args

    Usage:

      licenses_used(
          name = "license_info",
          deps = [":my_app"],
          out = "license_info.json",
      )
    """
    if not out:
        out = name + ".json"
    _licenses_used(name = name, deps = deps, out = out, **kwargs)
