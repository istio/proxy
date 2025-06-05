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

"""Utility functions to inspect and manipulate target triples."""

# Maps (operating system, environment) pairs from target triples to the legacy
# Bazel core `apple_common.platform` values, since we still use some APIs that
# require these.
_TRIPLE_OS_TO_PLATFORM = {
    ("ios", None): apple_common.platform.ios_device,
    ("ios", "simulator"): apple_common.platform.ios_simulator,
    ("macos", None): apple_common.platform.macos,
    ("tvos", None): apple_common.platform.tvos_device,
    ("tvos", "simulator"): apple_common.platform.tvos_simulator,
    # TODO: Remove getattr use once we no longer support 6.x
    ("xros", None): getattr(apple_common.platform, "visionos_device", None),
    ("xros", "simulator"): getattr(apple_common.platform, "visionos_simulator", None),
    ("watchos", None): apple_common.platform.watchos_device,
    ("watchos", "simulator"): apple_common.platform.watchos_simulator,
}

def _bazel_apple_platform(target_triple):
    """Returns the `apple_common.platform` value for the given target triple."""

    # TODO: Remove once we no longer support 6.x
    if target_triples.unversioned_os(target_triple) == "xros" and not hasattr(
        apple_common.platform,
        "visionos_device",
    ):
        fail("visionOS requested but your version of bazel doesn't support it")

    return _TRIPLE_OS_TO_PLATFORM[(
        target_triples.unversioned_os(target_triple),
        target_triple.environment,
    )]

def _make(*, cpu, vendor, os, environment = None):
    """Creates a target triple struct from the given values.

    Args:
        cpu: The CPU of the triple (e.g., `x86_64` or `arm`).
        vendor: The vendor component of the triple (e.g., `apple` or
            `unknown`).
        os: The operating system or platform name of the triple (e.g., `macos`
            or `linux`).
        environment: The environment or ABI component of the triple, if it was
            present. If this argument is omitted, it defaults to `None`.

    Returns:
        A `struct` containing the fields `cpu`, `vendor`, `os`, and
        `environment` which correspond to the arguments passed to this function.
    """
    if not cpu or not vendor or not os:
        fail("A triple must have a non-empty CPU, vendor, and OS.")

    return struct(
        cpu = cpu,
        vendor = vendor,
        os = os,
        environment = environment,
    )

def _normalize_apple_cpu(cpu):
    """Normalizes the CPU component of an Apple target triple.

    This function is equivalent to `getArchForAppleTargetSpecificModuleTrple` in
    https://github.com/apple/swift/blob/main/lib/Basic/Platform.cpp.
    """
    if cpu in ("arm64", "aarch64"):
        return "arm64"
    if cpu in ("arm64_32", "aarch64_32"):
        return "arm64_32"
    if cpu in ("x86_64", "amd64"):
        return "x86_64"
    if cpu in ("i386", "i486", "i586", "i686", "i786", "i886", "i986"):
        return "i386"
    if not cpu:
        return "unknown"
    return cpu

def _normalize_apple_environment(environment):
    """Normalizes the environment component of an Apple target triple.

    This function is equivalent to
    `getEnvironmentForAppleTargetSpecificModuleTriple` in
    https://github.com/apple/swift/blob/main/lib/Basic/Platform.cpp.
    """
    if environment == "unknown" or not environment:
        return None
    return environment

def _normalize_apple_os(os, *, unversioned = False):
    """Normalizes the OS component of an Apple target triple.

    This function is equivalent to `getOSForAppleTargetSpecificModuleTriple` in
    https://github.com/apple/swift/blob/main/lib/Basic/Platform.cpp.
    """
    os_name, version = _split_os_version(os)
    if os_name in ("macos", "macosx", "darwin"):
        os_name = "macos"
    elif not os_name:
        os_name = "unknown"
    return os_name if unversioned else (os_name + version)

def _normalize_for_swift(triple, *, unversioned = False):
    """Normalizes a target triple for use with the Swift compiler.

    This function performs that normalization, as well as other normalization
    implemented in
    https://github.com/apple/swift/blob/main/lib/Basic/Platform.cpp. It is named
    _specifically_ `normalize_for_swift` to differentiate it from the behavior
    defined in the `llvm::Triple::normalize` method, which has slightly
    different semantics.

    Args:
        triple: A target triple struct, as returned by `target_triples.make` or
            `target_triples.parse`.
        unversioned: A Boolean value indicating whether any OS version number
            component should be removed from the triple, if present.

    Returns:
        A target triple struct containing the normalized triple.
    """
    os = _normalize_apple_os(triple.os, unversioned = unversioned)
    if os.startswith(("ios", "macos", "tvos", "visionos", "watchos")):
        environment = _normalize_apple_environment(triple.environment)
        cpu = _normalize_apple_cpu(triple.cpu)
        return _make(
            cpu = cpu,
            vendor = "apple",
            os = os,
            environment = environment,
        )

    return triple

def _parse(triple_string):
    """Parses a target triple string and returns its fields as a struct.

    Args:
        triple_string: A string representing a target triple.

    Returns:
        A `struct` containing the following fields:

        *   `cpu`: The CPU of the triple (e.g., `x86_64` or `arm`).
        *   `vendor`: The vendor component of the triple (e.g., `apple` or
            `unknown`).
        *   `os`: The operating system or platform name of the triple (e.g.,
            `darwin` or `linux`).
        *   `environment`: The environment or ABI component of the triple, if
            it was present. This component may be `None`.
    """
    components = triple_string.split("-")
    if len(components) < 3:
        fail("Invalid target triple: {}, this likely means you're using the wrong CC toolchain, make sure you include apple_support in your project".format(triple_string))
    return _make(
        cpu = components[0],
        vendor = components[1],
        os = components[2],
        environment = components[3] if len(components) > 3 else None,
    )

def _platform_name_for_swift(triple):
    """Returns the platform name used by Swift to refer to the triple.

    The platform name is used as the name of the subdirectory under the Swift
    resource directory where libraries and modules are stored. On some
    platforms, such as Apple operating systems, this name encodes both OS and
    environment information from the triple: for example,
    `x86_64-apple-ios-simulator` has a platform name of `iphonesimulator`
    (matching the platform name from Xcode).

    Args:
        triple: A target triple struct, as returned by `target_triples.make` or
            `target_triples.parse`.

    Returns:
        A string representing the platform name.
    """
    os = _normalize_apple_os(_unversioned_os(triple), unversioned = True)
    if os == "macos":
        return "macosx"

    is_simulator = (triple.environment == "simulator")
    if os == "ios":
        return "iphonesimulator" if is_simulator else "iphoneos"
    if os == "tvos":
        return "appletvsimulator" if is_simulator else "appletvos"
    if os == "visionos":
        return "visionossimulator" if is_simulator else "visionos"
    if os == "watchos":
        return "watchsimulator" if is_simulator else "watchos"

    # Fall back to the operating system name if we aren't one of the cases
    # covered above. If more platforms need to be supported in the future, add
    # them here.
    return os

def _str(triple):
    """Returns the string representation of the target triple.

    Args:
        triple: A target triple struct, as returned by `target_triples.make` or
            `target_triples.parse`.

    Returns:
        The string representation of the target triple.
    """
    result = "{}-{}-{}".format(triple.cpu, triple.vendor, triple.os)
    if triple.environment:
        result += "-{}".format(triple.environment)
    return result

def _split_os_version(os):
    """Splits the OS version number from the end of the given component.

    Args:
        os: The OS component of a target triple.

    Returns:
        A tuple containing two elements: the operating system name and the
        version number. If there was no version number, then the second element
        will be the empty string.
    """
    for index in range(len(os)):
        if os[index].isdigit():
            return (os[:index], os[index:])
    return (os, "")

def _unversioned_os(triple):
    """Returns the operating system of the triple without the version number."""
    return _split_os_version(triple.os)[0]

target_triples = struct(
    bazel_apple_platform = _bazel_apple_platform,
    make = _make,
    normalize_for_swift = _normalize_for_swift,
    parse = _parse,
    platform_name_for_swift = _platform_name_for_swift,
    str = _str,
    unversioned_os = _unversioned_os,
)
