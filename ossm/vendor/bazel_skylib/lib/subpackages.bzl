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

"""Skylib module containing common functions for working with native.subpackages()
"""
_SUBPACKAGES_SUPPORTED = hasattr(native, "subpackages")

def _supported():
    return _SUBPACKAGES_SUPPORTED

def _check_supported():
    if not _SUBPACKAGES_SUPPORTED:
        fail("native.subpackages not supported in this version of Bazel.")

def _all(exclude = [], allow_empty = False, fully_qualified = True):
    """List all direct subpackages of the current package regardless of directory depth.

    The returned list contains all subpackages, but not subpackages of subpackages.

    Example:
    Assuming the following BUILD files exist:

        BUILD
        foo/BUILD
        foo/sub/BUILD
        bar/BUILD
        baz/deep/dir/BUILD

    If the current package is '//' all() will return ['//foo', '//bar',
    '//baz/deep/dir'].  //foo/sub is not included because it is a direct
    subpackage of '//foo' not '//'

    NOTE: fail()s if native.subpackages() is not supported.

    Args:
      exclude:          see native.subpackages(exclude)
      allow_empty:      see native.subpackages(allow_empty)
      fully_qualified:  It true return fully qualified Labels for subpackages,
          otherwise returns subpackage path relative to current package.

    Returns:
      A mutable sorted list containing all sub-packages of the current Bazel
      package.
    """
    _check_supported()

    subs = native.subpackages(include = ["**"], exclude = exclude, allow_empty = allow_empty)
    if fully_qualified:
        return [_fully_qualified(s) for s in subs]

    return subs

def _fully_qualified(relative_path):
    return "//%s/%s" % (native.package_name(), relative_path)

def _exists(relative_path):
    """Checks to see if relative_path is a direct subpackage of the current package.

    Example:

        BUILD
        foo/BUILD
        foo/sub/BUILD

    If the current package is '//' (the top-level BUILD file):
        subpackages.exists("foo") == True
        subpackages.exists("foo/sub") == False
        subpackages.exists("bar") == False

    NOTE: fail()s if native.subpackages() is not supported in the current Bazel version.

    Args:
      relative_path: a path to a subpackage to test, must not be an absolute Label.

    Returns:
      True if 'relative_path' is a subpackage of the current package.
    """
    _check_supported()
    return relative_path in native.subpackages(include = [relative_path], allow_empty = True)

subpackages = struct(
    all = _all,
    exists = _exists,
    supported = _supported,
)
