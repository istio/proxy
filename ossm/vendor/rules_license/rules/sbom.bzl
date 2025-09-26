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
"""SBOM generation"""

load(
    "@rules_license//rules_gathering:gather_metadata.bzl",
    "gather_metadata_info",
    "gather_metadata_info_and_write",
    "write_metadata_info",
)
load(
    "@rules_license//rules_gathering:gathering_providers.bzl",
    "TransitiveLicensesInfo",
)

# This rule is proof of concept, and may not represent the final
# form of a rule for compliance validation.
def _generate_sbom_impl(ctx):
    # Gather all licenses and write information to one place

    licenses_file = ctx.actions.declare_file("_%s_licenses_info.json" % ctx.label.name)
    write_metadata_info(ctx, ctx.attr.deps, licenses_file)

    # Now turn the big blob of data into something consumable.
    inputs = [licenses_file]
    outputs = [ctx.outputs.out]
    args = ctx.actions.args()
    args.add("--licenses_info", licenses_file.path)
    args.add("--out", ctx.outputs.out.path)
    ctx.actions.run(
        mnemonic = "CreateSBOM",
        progress_message = "Creating SBOM for %s" % ctx.label,
        inputs = inputs,
        outputs = outputs,
        executable = ctx.executable._sbom_generator,
        arguments = [args],
    )
    return [
        DefaultInfo(files = depset(outputs)),
        OutputGroupInfo(licenses_file = depset([licenses_file])),
    ]

_generate_sbom = rule(
    implementation = _generate_sbom_impl,
    attrs = {
        "deps": attr.label_list(
            aspects = [gather_metadata_info],
        ),
        "out": attr.output(mandatory = True),
        "_sbom_generator": attr.label(
            default = Label("@rules_license//tools:write_sbom"),
            executable = True,
            allow_files = True,
            cfg = "exec",
        ),
    },
)

def generate_sbom(**kwargs):
    _generate_sbom(**kwargs)

def _manifest_impl(ctx):
    # Gather all licenses and make it available as deps for downstream rules
    # Additionally write the list of license filenames to a file that can
    # also be used as an input to downstream rules.
    licenses_file = ctx.actions.declare_file(ctx.attr.out.name)
    mappings = get_licenses_mapping(ctx.attr.deps, ctx.attr.warn_on_legacy_licenses)
    ctx.actions.write(
        output = licenses_file,
        content = "\n".join([",".join([f.path, p]) for (f, p) in mappings.items()]),
    )
    return [DefaultInfo(files = depset(mappings.keys()))]

_manifest = rule(
    implementation = _manifest_impl,
    doc = """Internal tmplementation method for manifest().""",
    attrs = {
        "deps": attr.label_list(
            doc = """List of targets to collect license files for.""",
            aspects = [gather_metadata_info],
        ),
        "out": attr.output(
            doc = """Output file.""",
            mandatory = True,
        ),
        "warn_on_legacy_licenses": attr.bool(default = False),
    },
)

def manifest(name, deps, out = None, **kwargs):
    if not out:
        out = name + ".manifest"

    _manifest(name = name, deps = deps, out = out, **kwargs)

def get_licenses_mapping(deps, warn = False):
    """Creates list of entries representing all licenses for the deps.

    Args:

      deps: a list of deps which should have TransitiveLicensesInfo providers.
            This requires that you have run the gather_licenses_info
            aspect over them

      warn: boolean, if true, display output about legacy targets that need
            update

    Returns:
      {File:package_name}
    """
    tls = []
    for dep in deps:
        lds = dep[TransitiveLicensesInfo].licenses
        tls.append(lds)

    ds = depset(transitive = tls)

    # Ignore any legacy licenses that may be in the report
    mappings = {}
    for lic in ds.to_list():
        if type(lic.license_text) == "File":
            mappings[lic.license_text] = lic.package_name
        elif warn:
            # buildifier: disable=print
            print("Legacy license %s not included, rule needs updating" % lic.license_text)

    return mappings
