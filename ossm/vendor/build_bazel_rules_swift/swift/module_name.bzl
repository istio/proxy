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

"""The global module name derivation algorithm used by rules_swift."""

load("@bazel_skylib//lib:types.bzl", "types")

def derive_swift_module_name(*args):
    """Returns a derived module name from the given build label.

    For targets whose module name is not explicitly specified, the module name
    is computed using the following algorithm:

    *   The package and name components of the label are considered separately.
        All _interior_ sequences of non-identifier characters (anything other
        than `a-z`, `A-Z`, `0-9`, and `_`) are replaced by a single underscore
        (`_`). Any leading or trailing non-identifier characters are dropped.
    *   If the package component is non-empty after the above transformation,
        it is joined with the transformed name component using an underscore.
        Otherwise, the transformed name is used by itself.
    *   If this would result in a string that begins with a digit (`0-9`), an
        underscore is prepended to make it identifier-safe.

    This mapping is intended to be fairly predictable, but not reversible.

    Args:
        *args: Either a single argument of type `Label`, or two arguments of
            type `str` where the first argument is the package name and the
            second argument is the target name.

    Returns:
        The module name derived from the label.
    """
    if (len(args) == 1 and
        hasattr(args[0], "package") and
        hasattr(args[0], "name")):
        label = args[0]
        package = label.package
        name = label.name
    elif (len(args) == 2 and
          types.is_string(args[0]) and
          types.is_string(args[1])):
        package = args[0]
        name = args[1]
    else:
        fail("derive_swift_module_name may only be called with a single " +
             "argument of type 'Label' or two arguments of type 'str'.")

    package_part = _module_name_safe(package.lstrip("//"))
    name_part = _module_name_safe(name)
    if package_part:
        module_name = package_part + "_" + name_part
    else:
        module_name = name_part
    if module_name[0].isdigit():
        module_name = "_" + module_name
    return module_name

def _module_name_safe(string):
    """Returns a transformation of `string` that is safe for module names."""
    result = ""
    saw_non_identifier_char = False
    for ch in string.elems():
        if ch.isalnum() or ch == "_":
            # If we're seeing an identifier character after a sequence of
            # non-identifier characters, append an underscore and reset our
            # tracking state before appending the identifier character.
            if saw_non_identifier_char:
                result += "_"
                saw_non_identifier_char = False
            result += ch
        elif result:
            # Only track this if `result` has content; this ensures that we
            # (intentionally) drop leading non-identifier characters instead of
            # adding a leading underscore.
            saw_non_identifier_char = True

    return result
