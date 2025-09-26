# Copyright 2021 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Internal APIs used to operate on package specs."""

def _parse_package_spec(*, package_spec, workspace_name):
    """Parses a package spec from a string into a structured form.

    Args:
        package_spec: A string that represents a possibly recursive package
            specification, with an optional exclusion marker in front.
        workspace_name: The default workspace name to apply to package specs
            whose labels do not include a workspace name preceded by `@`.

    Returns:
        A `struct` containing three fields; see the documentation for
        `parse_package_specs` for their definitions.
    """
    if package_spec.startswith("-"):
        excluded = True
        package_spec = package_spec[1:]
    else:
        excluded = False

    if package_spec.startswith("@"):
        workspace_name, slashes, package_spec = package_spec[1:].partition("//")
        if not slashes:
            fail(("A package list may only contain absolute labels " +
                  "(found '{}').").format(package_spec))

    if package_spec.startswith("//"):
        package_spec = package_spec[2:]
    else:
        fail(("A package list may only contain absolute labels " +
              "(found '{}').").format(package_spec))

    if package_spec == "...":
        match_subpackages = True
        package = ""
    elif package_spec.endswith("/..."):
        match_subpackages = True
        package = package_spec[:-4]
    else:
        match_subpackages = False
        package = package_spec

    if package.endswith(":__pkg__"):
        fail(("The package list should contain only the package name, " +
              "without a ':__pkg__' suffix (found '{}').").format(package))
    elif package.endswith(":__subpackages__"):
        fail(("To list a package and all of its subpackages, write the " +
              "package name followed by '/...', not the ':__subpackages__' " +
              "metatarget (found '{}').").format(package))
    elif ":" in package:
        fail(("A package list may only list packages, not targets " +
              "(found '{}').").format(package))

    return struct(
        excluded = excluded,
        match_subpackages = match_subpackages,
        package = package,
        workspace_name = workspace_name,
    )

def parse_package_specs(*, package_specs, workspace_name):
    """Parses a list of package specs from strings into a structured form.

    Args:
        package_specs: A list of strings that represent zero or more package
            specifications, each of which may be recursive and/or may have an
            exclusion marker in front.
        workspace_name: The default workspace name to apply to package specs
            whose labels do not include a workspace name preceded by `@`.

    Returns:
        A list of `struct`s, each containing four fields:

        *   `excluded`: A Boolean value indicating whether targets matching this
            package spec should be excluded instead of included.
        *   `match_subpackages`: A Boolean value indicating whether targets in
            subpackages of the package spec should also be included, instead of
            only targets in the package written in the package spec.
        *   `package`: The package part of the targets that match this package
            spec, without any leading workspace name or `//` prefix.
        *   `workspace_name`: The workspace name of the targets that match this
            package spec.
    """
    return [
        _parse_package_spec(
            package_spec = package_spec,
            workspace_name = workspace_name,
        )
        for package_spec in package_specs
    ]

def _label_matches_package_spec_ignoring_exclusion(*, label, package_spec):
    """Returns a value indicating whether the label matches the package spec.

    This method ignores whether the package spec is an exclusion or not, because
    exclusions short-circuit the entire package spec list search. This simply
    checks whether the label has the same workspace name and package part (or
    package part prefix, if recursive) as the package spec.

    Args:
        label: The `Label` to test for a match against the list of package
            specs.
        package_spec: A package spec (as returned by `parse_package_specs`) that
            represent the packages that should be included or excluded in the
            match.

    Returns:
        True if the label matches the package spec, ignoring the package spec's
        exclusion property.
    """
    package = label.package
    workspace_name = label.workspace_name

    if package_spec.match_subpackages:
        if workspace_name != package_spec.workspace_name:
            return False
        if not package_spec.package:
            return True
        return (
            package == package_spec.package or
            package.startswith(package_spec.package + "/")
        )
    else:
        return (
            workspace_name == package_spec.workspace_name and
            package == package_spec.package
        )

def label_matches_package_specs(*, label, package_specs):
    """Returns a value indicating if a label matches a list of package specs.

    Args:
        label: The `Label` to test for a match against the list of package
            specs.
        package_specs: A list of package specs (as returned by
            `parse_package_specs`) that represent the packages that should be
            included or excluded in the match.

    Returns:
        True if the label matches the package specs; i.e., it has the same
        workspace name and package part, or package part prefix in the recursive
        case) as one of the specs in the list, and it does not match any of the
        exclusions in the list.
    """
    at_least_one_match = False

    for package_spec in package_specs:
        is_match = _label_matches_package_spec_ignoring_exclusion(
            label = label,
            package_spec = package_spec,
        )
        if is_match:
            # Package exclusions always take precedence over package inclusions,
            # so if we have an exclusion match, return false immediately.
            # Otherwise, we track that we have a match but we must continue to
            # search the list because an exclusion could come later that also
            # matches.
            if package_spec.excluded:
                return False
            else:
                at_least_one_match = True

    return at_least_one_match
