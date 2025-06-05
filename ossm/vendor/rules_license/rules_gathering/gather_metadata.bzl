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
    "@rules_license//rules:providers.bzl",
    "ExperimentalMetadataInfo",
    "PackageInfo",
)
load(
    "@rules_license//rules_gathering:gathering_providers.bzl",
    "TransitiveMetadataInfo",
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

def _bazel_package(label):
    clean_label = _strip_null_repo(label)
    return clean_label[0:-(len(label.name) + 1)]

def _gather_metadata_info_impl(target, ctx):
    return gather_metadata_info_common(
        target,
        ctx,
        TransitiveMetadataInfo,
        [ExperimentalMetadataInfo, PackageInfo],
        should_traverse)

gather_metadata_info = aspect(
    doc = """Collects LicenseInfo providers into a single TransitiveMetadataInfo provider.""",
    implementation = _gather_metadata_info_impl,
    attr_aspects = ["*"],
    attrs = {
        "_trace": attr.label(default = "@rules_license//rules:trace_target"),
    },
    provides = [TransitiveMetadataInfo],
    apply_to_generating_rules = True,
)

def _write_metadata_info_impl(target, ctx):
    """Write transitive license info into a JSON file

    Args:
      target: The target of the aspect.
      ctx: The aspect evaluation context.

    Returns:
      OutputGroupInfo
    """

    if not TransitiveMetadataInfo in target:
        return [OutputGroupInfo(licenses = depset())]
    info = target[TransitiveMetadataInfo]
    outs = []

    # If the result doesn't contain licenses, we simply return the provider
    if not hasattr(info, "target_under_license"):
        return [OutputGroupInfo(licenses = depset())]

    # Write the output file for the target
    name = "%s_metadata_info.json" % ctx.label.name
    content = "[\n%s\n]\n" % ",\n".join(metadata_info_to_json(info))
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

gather_metadata_info_and_write = aspect(
    doc = """Collects TransitiveMetadataInfo providers and writes JSON representation to a file.

    Usage:
      bazel build //some:target \
          --aspects=@rules_license//rules_gathering:gather_metadata.bzl%gather_metadata_info_and_write
          --output_groups=licenses
    """,
    implementation = _write_metadata_info_impl,
    attr_aspects = ["*"],
    attrs = {
        "_trace": attr.label(default = "@rules_license//rules:trace_target"),
    },
    provides = [OutputGroupInfo],
    requires = [gather_metadata_info],
    apply_to_generating_rules = True,
)

def write_metadata_info(ctx, deps, json_out):
    """Writes TransitiveMetadataInfo providers for a set of targets as JSON.

    TODO(aiuto): Document JSON schema. But it is under development, so the current
    best place to look is at tests/hello_licenses.golden.

    Usage:
      write_metadata_info must be called from a rule implementation, where the
      rule has run the gather_metadata_info aspect on its deps to
      collect the transitive closure of LicenseInfo providers into a
      LicenseInfo provider.

      foo = rule(
        implementation = _foo_impl,
        attrs = {
           "deps": attr.label_list(aspects = [gather_metadata_info])
        }
      )

      def _foo_impl(ctx):
        ...
        out = ctx.actions.declare_file("%s_licenses.json" % ctx.label.name)
        write_metadata_info(ctx, ctx.attr.deps, metadata_file)

    Args:
      ctx: context of the caller
      deps: a list of deps which should have TransitiveMetadataInfo providers.
            This requires that you have run the gather_metadata_info
            aspect over them
      json_out: output handle to write the JSON info
    """
    licenses = []
    for dep in deps:
        if TransitiveMetadataInfo in dep:
            licenses.extend(metadata_info_to_json(dep[TransitiveMetadataInfo]))
    ctx.actions.write(
        output = json_out,
        content = "[\n%s\n]\n" % ",\n".join(licenses),
    )

def metadata_info_to_json(metadata_info):
    """Render a single LicenseInfo provider to JSON

    Args:
      metadata_info: A LicenseInfo.

    Returns:
      [(str)] list of LicenseInfo values rendered as JSON.
    """

    main_template = """  {{
    "top_level_target": "{top_level_target}",
    "dependencies": [{dependencies}
    ],
    "licenses": [{licenses}
    ],
    "packages": [{packages}
    ]\n  }}"""

    dep_template = """
      {{
        "target_under_license": "{target_under_license}",
        "licenses": [
          {licenses}
        ]
      }}"""

    license_template = """
      {{
        "label": "{label}",
        "bazel_package": "{bazel_package}",
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
            "conditions": {kind_conditions}
          }}"""

    package_info_template = """
          {{
            "target": "{label}",
            "bazel_package": "{bazel_package}",
            "package_name": "{package_name}",
            "package_url": "{package_url}",
            "package_version": "{package_version}",
            "purl": "{purl}"
          }}"""

    # Build reverse map of license to user
    used_by = {}
    for dep in metadata_info.deps.to_list():
        # Undo the concatenation applied when stored in the provider.
        dep_licenses = dep.licenses.split(",")
        for license in dep_licenses:
            if license not in used_by:
                used_by[license] = []
            used_by[license].append(_strip_null_repo(dep.target_under_license))

    all_licenses = []
    for license in sorted(metadata_info.licenses.to_list(), key = lambda x: x.label):
        kinds = []
        for kind in sorted(license.license_kinds, key = lambda x: x.name):
            kinds.append(kind_template.format(
                kind_name = kind.name,
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
                bazel_package =  _bazel_package(license.label),
                used_by = ",\n          ".join(sorted(['"%s"' % x for x in used_by[str(license.label)]])),
            ))

    all_deps = []
    for dep in sorted(metadata_info.deps.to_list(), key = lambda x: x.target_under_license):
        # Undo the concatenation applied when stored in the provider.
        dep_licenses = dep.licenses.split(",")
        all_deps.append(dep_template.format(
            target_under_license = _strip_null_repo(dep.target_under_license),
            licenses = ",\n          ".join(sorted(['"%s"' % _strip_null_repo(x) for x in dep_licenses])),
        ))

    all_packages = []
    # We would use this if we had distinct depsets for every provider type.
    #for package in sorted(metadata_info.package_info.to_list(), key = lambda x: x.label):
    #    all_packages.append(package_info_template.format(
    #        label = _strip_null_repo(package.label),
    #        package_name = package.package_name,
    #        package_url = package.package_url,
    #        package_version = package.package_version,
    #    ))

    for mi in sorted(metadata_info.other_metadata.to_list(), key = lambda x: x.label):
        # Maybe use a map of provider class to formatter.  A generic dict->json function
        # in starlark would help

        # This format is for using distinct providers.  I like the compile time safety.
        if mi.type == "package_info":
            all_packages.append(package_info_template.format(
                label = _strip_null_repo(mi.label),
                bazel_package =  _bazel_package(mi.label),
                package_name = mi.package_name,
                package_url = mi.package_url,
                package_version = mi.package_version,
                purl = mi.purl,
            ))
        # experimental: Support the ExperimentalMetadataInfo bag of data
        # WARNING: Do not depend on this. It will change without notice.
        if mi.type == "package_info_alt":
            all_packages.append(package_info_template.format(
                label = _strip_null_repo(mi.label),
                bazel_package =  _bazel_package(mi.label),
                # data is just a bag, so we need to use get() or ""
                package_name = mi.data.get("package_name") or "",
                package_url = mi.data.get("package_url") or "",
                package_version = mi.data.get("package_version") or "",
                purl = mi.data.get("purl") or "",
            ))

    return [main_template.format(
        top_level_target = _strip_null_repo(metadata_info.target_under_license),
        dependencies = ",".join(all_deps),
        licenses = ",".join(all_licenses),
        packages = ",".join(all_packages),
    )]
