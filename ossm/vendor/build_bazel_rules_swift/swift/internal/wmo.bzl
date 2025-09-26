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

"""Functionality releated to detecting whole-module optimization."""

load(
    ":feature_names.bzl",
    "SWIFT_FEATURE__NUM_THREADS_0_IN_SWIFTCOPTS",
    "SWIFT_FEATURE__WMO_IN_SWIFTCOPTS",
)

# Swift command line flags that enable whole module optimization. (This
# dictionary is used as a set for quick lookup; the values are irrelevant.)
_WMO_FLAGS = {
    "-wmo": True,
    "-whole-module-optimization": True,
    "-force-single-frontend-invocation": True,
}

def features_from_swiftcopts(swiftcopts):
    """Returns a list of features to enable based on `--swiftcopt` flags.

    Since `--swiftcopt` flags are hooked into the action configuration when the
    toolchain is configured, it's not possible for individual actions to query
    them easily if those flags may determine the nature of outputs (for example,
    single- vs. multi-threaded WMO). The toolchain can call this function to map
    those flags to private features that can be queried instead.

    Args:
        swiftcopts: The list of command line flags that were passed using
            `--swiftcopt`.

    Returns:
        A list (possibly empty) of strings denoting feature names that should be
        enabled on the toolchain.
    """
    features = []
    if is_wmo_manually_requested(user_compile_flags = swiftcopts):
        features.append(SWIFT_FEATURE__WMO_IN_SWIFTCOPTS)
    if find_num_threads_flag_value(user_compile_flags = swiftcopts) == 0:
        features.append(SWIFT_FEATURE__NUM_THREADS_0_IN_SWIFTCOPTS)
    return features

def find_num_threads_flag_value(user_compile_flags):
    """Finds the value of the `-num-threads` flag.

    This function looks for the `-num-threads` flag and returns the
    corresponding value if found. If the flag is present multiple times, the
    last value is the one returned.

    Args:
        user_compile_flags: The options passed into the compile action.

    Returns:
        The numeric value of the `-num-threads` flag if found, otherwise `None`.
    """
    num_threads = None
    saw_num_threads = False
    for copt in user_compile_flags:
        if saw_num_threads:
            saw_num_threads = False
            num_threads = _safe_int(copt)
        elif copt == "-num-threads":
            saw_num_threads = True
    return num_threads

def is_wmo_manually_requested(user_compile_flags):
    """Returns `True` if a WMO flag is in the given list of compiler flags.

    Args:
        user_compile_flags: A list of compiler flags to scan for WMO usage.

    Returns:
        True if WMO is enabled in the given list of flags.
    """
    for copt in user_compile_flags:
        if copt in _WMO_FLAGS:
            return True
    return False

def wmo_features_from_swiftcopts(swiftcopts):
    """Returns a list of features to enable based on `--swiftcopt` flags.

    Since `--swiftcopt` flags are hooked into the action configuration when the
    toolchain is configured, it's not possible for individual actions to query
    them easily if those flags may determine the nature of outputs (for example,
    single- vs. multi-threaded WMO). The toolchain can call this function to map
    those flags to private features that can be queried instead.

    Args:
        swiftcopts: The list of command line flags that were passed using
            `--swiftcopt`.

    Returns:
        A list (possibly empty) of strings denoting feature names that should be
        enabled on the toolchain.
    """
    features = []
    if is_wmo_manually_requested(user_compile_flags = swiftcopts):
        features.append(SWIFT_FEATURE__WMO_IN_SWIFTCOPTS)
    if find_num_threads_flag_value(user_compile_flags = swiftcopts) == 1:
        features.append(SWIFT_FEATURE__NUM_THREADS_0_IN_SWIFTCOPTS)
    return features

def _safe_int(s):
    """Returns the base-10 integer value of `s` or `None` if it is invalid.

    This function is needed because `int()` fails the build when passed a string
    that isn't a valid integer, with no way to recover
    (https://github.com/bazelbuild/bazel/issues/5940).

    Args:
        s: The string to be converted to an integer.

    Returns:
        The integer value of `s`, or `None` if was not a valid base 10 integer.
    """
    for i in range(len(s)):
        if s[i] < "0" or s[i] > "9":
            return None
    return int(s)
