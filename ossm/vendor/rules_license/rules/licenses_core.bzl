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

load("@rules_license//rules:filtered_rule_kinds.bzl", "aspect_filters")
load("@rules_license//rules:providers.bzl", "LicenseInfo")
load("@rules_license//rules:user_filtered_rule_kinds.bzl", "user_aspect_filters")
load(
    "@rules_license//rules_gathering:gathering_providers.bzl",
    "LicensedTargetInfo",
    "TransitiveLicensesInfo",
)
load("@rules_license//rules_gathering:trace.bzl", "TraceInfo")

def should_traverse(ctx, attr):
    """Checks if the dependent attribute should be traversed.

    Args:
      ctx: The aspect evaluation context.
      attr: The name of the attribute to be checked.

    Returns:
      True iff the attribute should be traversed.
    """
    k = ctx.rule.kind

    for filters in [aspect_filters, user_aspect_filters]:
        always_ignored = filters.get("*", [])
        if k in filters:
            attr_matches = filters[k]
            if (attr in attr_matches or
                "*" in attr_matches or
                ("_*" in attr_matches and attr.startswith("_")) or
                attr in always_ignored):
                return False

            for m in attr_matches:
                if attr == m:
                    return False

    return True

def _get_transitive_metadata(ctx, trans_licenses, trans_other_metadata, trans_package_info, trans_deps, traces, provider, filter_func):
    attrs = [a for a in dir(ctx.rule.attr)]
    for name in attrs:
        if not filter_func(ctx, name):
            continue
        a = getattr(ctx.rule.attr, name)

        # Make anything singleton into a list for convenience.
        if type(a) != type([]):
            a = [a]
        for dep in a:
            # Ignore anything that isn't a target
            if type(dep) != "Target":
                continue

            # Targets can also include things like input files that won't have the
            # aspect, so we additionally check for the aspect rather than assume
            # it's on all targets.  Even some regular targets may be synthetic and
            # not have the aspect. This provides protection against those outlier
            # cases.
            if provider in dep:
                info = dep[provider]
                if info.licenses:
                    trans_licenses.append(info.licenses)
                if info.deps:
                    trans_deps.append(info.deps)
                if info.traces:
                    for trace in info.traces:
                        traces.append("(" + ", ".join([str(ctx.label), ctx.rule.kind, name]) + ") -> " + trace)

                # We only need one or the other of these stanzas.
                # If we use a polymorphic approach to metadata providers, then
                # this works.
                if hasattr(info, "other_metadata"):
                    if info.other_metadata:
                        trans_other_metadata.append(info.other_metadata)

                # But if we want more precise type safety, we would have a
                # trans_* for each type of metadata. That is not user
                # extensibile.
                if hasattr(info, "package_info"):
                    if info.package_info:
                        trans_package_info.append(info.package_info)

def gather_metadata_info_common(target, ctx, provider_factory, metadata_providers, filter_func):
    """Collect license and other metadata info from myself and my deps.

    Any single target might directly depend on a license, or depend on
    something that transitively depends on a license, or neither.
    This aspect bundles all those into a single provider. At each level, we add
    in new direct license deps found and forward up the transitive information
    collected so far.

    This is a common abstraction for crawling the dependency graph. It is
    parameterized to allow specifying the provider that is populated with
    results. It is configurable to select only a subset of providers. It
    is also configurable to specify which dependency edges should not
    be traced for the purpose of tracing the graph.

    Args:
      target: The target of the aspect.
      ctx: The aspect evaluation context.
      provider_factory: abstracts the provider returned by this aspect
      metadata_providers: a list of other providers of interest
      filter_func: a function that returns true iff the dep edge should be ignored

    Returns:
      provider of parameterized type
    """

    # First we gather my direct license attachments
    licenses = []
    other_metadata = []
    package_info = []
    if ctx.rule.kind == "_license":
        # Don't try to gather licenses from the license rule itself. We'll just
        # blunder into the text file of the license and pick up the default
        # attribute of the package, which we don't want.
        pass
    else:
        if hasattr(ctx.rule.attr, "applicable_licenses"):
            package_metadata = ctx.rule.attr.applicable_licenses
        elif hasattr(ctx.rule.attr, "package_metadata"):
            package_metadata = ctx.rule.attr.package_metadata
        else:
            package_metadata = []

        for dep in package_metadata:
            if LicenseInfo in dep:
                lic = dep[LicenseInfo]
                licenses.append(lic)

            for m_p in metadata_providers:
                if m_p in dep:
                    other_metadata.append(dep[m_p])

    # A hack until https://github.com/bazelbuild/rules_license/issues/89 is
    # fully resolved. If exec is in the bin_dir path, then the current
    # configuration is probably cfg = exec.
    if "-exec-" in ctx.bin_dir.path:
        return [provider_factory(deps = depset(), licenses = depset(), traces = [])]

    # Now gather transitive collection of providers from the targets
    # this target depends upon.
    trans_licenses = []
    trans_other_metadata = []
    trans_package_info = []
    trans_deps = []
    traces = []

    _get_transitive_metadata(ctx, trans_licenses, trans_other_metadata, trans_package_info, trans_deps, traces, provider_factory, filter_func)

    if not licenses and not trans_licenses:
        return [provider_factory(deps = depset(), licenses = depset(), traces = [])]

    # If this is the target, start the sequence of traces.
    if ctx.attr._trace[TraceInfo].trace and ctx.attr._trace[TraceInfo].trace in str(ctx.label):
        traces = [ctx.attr._trace[TraceInfo].trace]

    # Trim the number of traces accumulated since the output can be quite large.
    # A few representative traces are generally sufficient to identify why a dependency
    # is incorrectly incorporated.
    if len(traces) > 10:
        traces = traces[0:10]

    if licenses:
        # At this point we have a target and a list of directly used licenses.
        # Bundle those together so we can report the exact targets that cause the
        # dependency on each license. Since a list cannot be stored in a
        # depset, even inside a provider, the list is concatenated into a
        # string and will be unconcatenated in the output phase.
        direct_license_uses = [LicensedTargetInfo(
            target_under_license = target.label,
            licenses = ",".join([str(x.label) for x in licenses]),
        )]
    else:
        direct_license_uses = None

    # This is a bit of a hack for bazel 5.x.  We can not pass extra fields to
    # the provider constructor, so we need to do something special for each.
    # In Bazel 6.x we can use a provider initializer function that would take
    # all the args and only use the ones it wants.
    if provider_factory == TransitiveLicensesInfo:
        return [provider_factory(
            target_under_license = target.label,
            licenses = depset(tuple(licenses), transitive = trans_licenses),
            deps = depset(direct = direct_license_uses, transitive = trans_deps),
            traces = traces,
        )]

    return [provider_factory(
        target_under_license = target.label,
        licenses = depset(tuple(licenses), transitive = trans_licenses),
        other_metadata = depset(tuple(other_metadata), transitive = trans_other_metadata),
        deps = depset(direct = direct_license_uses, transitive = trans_deps),
        traces = traces,
    )]
