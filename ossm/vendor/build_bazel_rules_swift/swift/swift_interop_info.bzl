# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Defines the public interface for creating a lightweight interop provider.

Note that the only way for external users to create this provider is by calling
the `create_swift_interop_info` function. This is because it is meant to be a
"write-only" provider; one that targets can propagate but should not attempt to
read.

**NOTE:** This file intentionally contains no `load` statements other than the
one that defines the provider, which also contains no `load` statements. It is
loaded by the `swift_interop_hint` rule, and packages loading that rule often
contain no other Swift code and load no other Swift rules. The purpose of
avoiding unnecessary loads in this file and in `swift_interop_hint.bzl` is to
minimize build graph invalidations among those packages when other, unrelated
parts of the Swift rules change.
"""

load("//swift/internal:swift_interop_info.bzl", "SwiftInteropInfo")

def create_swift_interop_info(
        *,
        exclude_headers = [],
        module_map = None,
        module_name = None,
        requested_features = [],
        suppressed = False,
        swift_infos = [],
        unsupported_features = []):
    """Returns a provider that lets a target expose C/Objective-C APIs to Swift.

    The provider returned by this function allows custom build rules written in
    Starlark to be uninvolved with much of the low-level machinery involved in
    making a Swift-compatible module. Such a target should propagate a `CcInfo`
    provider whose compilation context contains the headers that it wants to
    make into a module, and then also propagate the provider returned from this
    function.

    The simplest usage is for a custom rule to do the following:

    *   Add `swift_clang_module_aspect` to any attribute that provides
        dependencies of the code that needs to interop with Swift (typically
        `deps`, but could be other attributes as well, such as attributes
        providing additional support libraries).
    *   Have the rule implementation call `create_swift_interop_info`, passing
        it only the list of `SwiftInfo` providers from its dependencies. This
        tells `swift_clang_module_aspect` when it runs on *this* rule's target
        to derive the module name from the target label and create a module map
        using the headers from the compilation context of the `CcInfo` you
        propagate.

    If the custom rule has reason to provide its own module name or module map,
    then it can do so using the `module_name` and `module_map` arguments.

    When a rule returns this provider, it must provide the full set of
    `SwiftInfo` providers from dependencies that will be merged with the one
    that `swift_clang_module_aspect` creates for the target itself. The aspect
    will **not** collect dependency providers automatically. This allows the
    rule to not only add extra dependencies (such as support libraries from
    implicit attributes) but also to exclude dependencies if necessary.

    Args:
        exclude_headers: A `list` of `File`s representing headers that should be
            excluded from the module if the module map is generated.
        module_map: A `File` representing an existing module map that should be
            used to represent the module, or `None` (the default) if the module
            map should be generated based on the headers in the target's
            compilation context. If this argument is provided, then
            `module_name` must also be provided.
        module_name: A string denoting the name of the module, or `None` (the
            default) if the name should be derived automatically from the target
            label.
        requested_features: A list of features (empty by default) that should be
            requested for the target, which are added to those supplied in the
            `features` attribute of the target. These features will be enabled
            unless they are otherwise marked as unsupported (either on the
            target or by the toolchain). This allows the rule implementation to
            have additional control over features that should be supported by
            default for all instances of that rule as if it were creating the
            feature configuration itself; for example, a rule can request that
            `swift.emit_c_module` always be enabled for its targets even if it
            is not explicitly enabled in the toolchain or on the target
            directly.
        suppressed: A `bool` indicating whether the module that the aspect would
            create for the target should instead be suppressed.
        swift_infos: A list of `SwiftInfo` providers from dependencies, which
            will be merged with the new `SwiftInfo` created by the aspect.
        unsupported_features: A list of features (empty by default) that should
            be considered unsupported for the target, which are added to those
            supplied as negations in the `features` attribute. This allows the
            rule implementation to have additional control over features that
            should be disabled by default for all instances of that rule as if
            it were creating the feature configuration itself; for example, a
            rule that processes frameworks with headers that do not follow
            strict layering can request that `swift.strict_module` always be
            disabled for its targets even if it is enabled by default in the
            toolchain.

    Returns:
        A provider whose type/layout is an implementation detail and should not
        be relied upon.
    """
    if module_map:
        if not module_name:
            fail("'module_name' must be specified when 'module_map' is " +
                 "specified.")
        if exclude_headers:
            fail("'exclude_headers' may not be specified when 'module_map' " +
                 "is specified.")

    return SwiftInteropInfo(
        exclude_headers = exclude_headers,
        module_map = module_map,
        module_name = module_name,
        requested_features = requested_features,
        suppressed = suppressed,
        swift_infos = swift_infos,
        unsupported_features = unsupported_features,
    )
