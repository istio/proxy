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
"""Rules and macros for collecting LicenseInfo providers."""

load(
    "@rules_license//rules:licenses_core.bzl",
    "gather_metadata_info_common",
    "should_traverse",
)
load(
    "@rules_license//rules_gathering:gathering_providers.bzl",
    "TransitiveLicensesInfo",
)
load("@rules_license//rules_gathering:trace.bzl", "TraceInfo")

def _strip_null_repo(label):
    """Removes the null repo name (e.g. @//) from a string.

    The is to make str(label) compatible between bazel 5.x and 6.x
    """
    s = str(label)
    if s.startswith('@//'):
        return s[1:]
    elif s.startswith('@@//'):
        return s[2:]
    return s

def _gather_licenses_info_impl(target, ctx):
    return gather_metadata_info_common(target, ctx, TransitiveLicensesInfo, [], should_traverse)

gather_licenses_info = aspect(
    doc = """Collects LicenseInfo providers into a single TransitiveLicensesInfo provider.""",
    implementation = _gather_licenses_info_impl,
    attr_aspects = ["*"],
    attrs = {
        "_trace": attr.label(default = "@rules_license//rules:trace_target"),
    },
    provides = [TransitiveLicensesInfo],
    apply_to_generating_rules = True,
)

def _write_licenses_info_impl(target, ctx):
    """Write transitive license info into a JSON file

    Args:
      target: The target of the aspect.
      ctx: The aspect evaluation context.

    Returns:
      OutputGroupInfo
    """

    if not TransitiveLicensesInfo in target:
        return [OutputGroupInfo(licenses = depset())]
    info = target[TransitiveLicensesInfo]
    outs = []

    # If the result doesn't contain licenses, we simply return the provider
    if not hasattr(info, "target_under_license"):
        return [OutputGroupInfo(licenses = depset())]

    # Write the output file for the target
    name = "%s_licenses_info.json" % ctx.label.name
    lic_info, _ = licenses_info_to_json(info)
    content = "[\n%s\n]\n" % ",\n".join(lic_info)
    out = ctx.actions.declare_file(name)
    ctx.actions.write(
        output = out,
        content = content,
    )
    outs.append(out)

    if ctx.attr._trace[TraceInfo].trace:
        trace = ctx.actions.declare_file("%s_trace_info.json" % ctx.label.name)
        ctx.actions.write(output = trace, content = "\n".join(info.traces))
        outs.append(trace)

    return [OutputGroupInfo(licenses = depset(outs))]

gather_licenses_info_and_write = aspect(
    doc = """Collects TransitiveLicensesInfo providers and writes JSON representation to a file.

    Usage:
      blaze build //some:target \
          --aspects=@rules_license//rules:gather_licenses_info.bzl%gather_licenses_info_and_write
          --output_groups=licenses
    """,
    implementation = _write_licenses_info_impl,
    attr_aspects = ["*"],
    attrs = {
        "_trace": attr.label(default = "@rules_license//rules:trace_target"),
    },
    provides = [OutputGroupInfo],
    requires = [gather_licenses_info],
    apply_to_generating_rules = True,
)

def write_licenses_info(ctx, deps, json_out):
    """Writes TransitiveLicensesInfo providers for a set of targets as JSON.

    TODO(aiuto): Document JSON schema. But it is under development, so the current
    best place to look is at tests/hello_licenses.golden.

    Usage:
      write_licenses_info must be called from a rule implementation, where the
      rule has run the gather_licenses_info aspect on its deps to
      collect the transitive closure of LicenseInfo providers into a
      LicenseInfo provider.

      foo = rule(
        implementation = _foo_impl,
        attrs = {
           "deps": attr.label_list(aspects = [gather_licenses_info])
        }
      )

      def _foo_impl(ctx):
        ...
        json_file = ctx.actions.declare_file("%s_licenses.json" % ctx.label.name)
        license_files = write_licenses_info(ctx, ctx.attr.deps, json_file)

        // process the json file and the license_files referenced by it
        ctx.actions.run(
          inputs = [json_file] + license_files
          executable = ...
        )

    Args:
      ctx: context of the caller
      deps: a list of deps which should have TransitiveLicensesInfo providers.
            This requires that you have run the gather_licenses_info
            aspect over them
      json_out: output handle to write the JSON info

    Returns:
      A list of License File objects for each of the license paths referenced in the json.
    """
    licenses_json = []
    licenses_files = []
    for dep in deps:
        if TransitiveLicensesInfo in dep:
            transitive_licenses_info = dep[TransitiveLicensesInfo]            
            lic_info, _ = licenses_info_to_json(transitive_licenses_info)
            licenses_json.extend(lic_info)
            for info in transitive_licenses_info.licenses.to_list():
                if info.license_text:
                    licenses_files.append(info.license_text)

    ctx.actions.write(
        output = json_out,
        content = "[\n%s\n]\n" % ",\n".join(licenses_json),
    )
    return licenses_files

def licenses_info_to_json(licenses_info):
    """Render a single LicenseInfo provider to JSON

    Args:
      licenses_info: A LicenseInfo.

    Returns:
      [(str)] list of LicenseInfo values rendered as JSON.
      [(File)] list of Files containing license texts.
    """

    main_template = """  {{
    "top_level_target": "{top_level_target}",
    "dependencies": [{dependencies}
    ],
    "licenses": [{licenses}
    ]\n  }}"""

    dep_template = """
      {{
        "target_under_license": "{target_under_license}",
        "licenses": [
          {licenses}
        ]
      }}"""

    # TODO(aiuto): 'rule' is a duplicate of 'label' until old users are transitioned
    license_template = """
      {{
        "label": "{label}",
        "rule": "{label}",
        "license_kinds": [{kinds}
        ],
        "copyright_notice": "{copyright_notice}",
        "package_name": "{package_name}",
        "package_url": "{package_url}",
        "package_version": "{package_version}",
        "license_text": "{license_text}",
        "used_by": [
          {used_by}
        ]
      }}"""

    kind_template = """
          {{
            "target": "{kind_path}",
            "name": "{kind_name}",
            "long_name": "{kind_long_name}",
            "conditions": {kind_conditions}
          }}"""

    # Build reverse map of license to user
    used_by = {}
    for dep in licenses_info.deps.to_list():
        # Undo the concatenation applied when stored in the provider.
        dep_licenses = dep.licenses.split(",")
        for license in dep_licenses:
            if license not in used_by:
                used_by[license] = []
            used_by[license].append(_strip_null_repo(dep.target_under_license))

    all_licenses = []
    all_license_text_files = []
    for license in sorted(licenses_info.licenses.to_list(), key = lambda x: x.label):
        kinds = []
        for kind in sorted(license.license_kinds, key = lambda x: x.name):
            if hasattr(kind, "long_name"):
                long_name = kind.long_name
            else:
                long_name = ""
            kinds.append(kind_template.format(
                kind_name = kind.name,
                kind_long_name = long_name,
                kind_path = kind.label,
                kind_conditions = kind.conditions,
            ))

        if license.license_text:
            # Special handling for synthetic LicenseInfo
            text_path = (license.license_text.package + "/" + license.license_text.name if type(license.license_text) == "Label" else license.license_text.path)
            all_licenses.append(license_template.format(
                copyright_notice = license.copyright_notice,
                kinds = ",".join(kinds),
                license_text = text_path,
                package_name = license.package_name,
                package_url = license.package_url,
                package_version = license.package_version,
                label = _strip_null_repo(license.label),
                used_by = ",\n          ".join(sorted(['"%s"' % x for x in used_by[str(license.label)]])),
            ))
            # Additionally return all File references so that other rules invoking
            # this method can load license text file contents from external repos
            # using runfiles
            all_license_text_files.append(license.license_text)
    all_deps = []
    for dep in sorted(licenses_info.deps.to_list(), key = lambda x: x.target_under_license):
        # Undo the concatenation applied when stored in the provider.
        dep_licenses = dep.licenses.split(",")
        all_deps.append(dep_template.format(
            target_under_license = _strip_null_repo(dep.target_under_license),
            licenses = ",\n          ".join(sorted(['"%s"' % _strip_null_repo(x) for x in dep_licenses])),
        ))

    return [main_template.format(
        top_level_target = _strip_null_repo(licenses_info.target_under_license),
        dependencies = ",".join(all_deps),
        licenses = ",".join(all_licenses),
    )], all_license_text_files
