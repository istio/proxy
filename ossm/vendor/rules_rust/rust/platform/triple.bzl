"""Triples are a way to define information about a platform/system. This module provides
a way to convert a triple string into a well structured object to avoid constant string
parsing in starlark code, and a way for a repository_rule to extract the target triple
of the host platform.

Triples can be described at the following link:
https://clang.llvm.org/docs/CrossCompilation.html#target-triple
"""

def triple(triple):
    """Constructs a struct containing each component of the provided triple

    Args:
        triple (str): A platform triple. eg: `x86_64-unknown-linux-gnu`

    Returns:
        struct:
            - arch (str): The triple's CPU architecture
            - vendor (str): The vendor of the system
            - system (str): The name of the system
            - abi (str, optional): The abi to use or None if abi does not apply.
            - str (str): Original string representation of the triple
    """
    if triple in ("wasm32-wasi", "wasm32-wasip1"):
        trip = triple
        if trip == "wasm32-wasi":
            trip = "wasm32-wasip1"
        return struct(
            arch = trip.split("-")[0],
            vendor = trip.split("-")[1],
            system = trip.split("-")[1],
            abi = None,
            str = trip,
        )
    elif triple in ("aarch64-fuchsia", "x86_64-fuchsia"):
        return struct(
            arch = triple.split("-")[0],
            vendor = "unknown",
            system = "fuchsia",
            abi = None,
            str = triple,
        )

    component_parts = triple.split("-")
    if len(component_parts) < 3:
        fail("Expected target triple to contain at least three sections separated by '-'")

    cpu_arch = component_parts[0]
    vendor = component_parts[1]
    system = component_parts[2]
    abi = None

    if cpu_arch.startswith(("thumbv8m", "thumbv7m", "thumbv7e", "thumbv6m")):
        abi = system
        system = vendor
        vendor = None

    if system == "androideabi":
        system = "android"
        abi = "eabi"

    if len(component_parts) == 4:
        abi = component_parts[3]

    return struct(
        arch = cpu_arch,
        vendor = vendor,
        system = system,
        abi = abi,
        str = triple,
    )

def _validate_cpu_architecture(arch, expected_archs):
    """Validate the host CPU architecture

    Args:
        arch (string): a CPU architecture
        expected_archs (list): A list of expected architecture strings
    """
    if arch not in expected_archs:
        fail("{} is not a expected cpu architecture {}".format(
            arch,
            expected_archs,
        ))

def get_host_triple(repository_ctx, abi = None):
    """Query host information for the appropriate triple to use with load_arbitrary_tool or the crate_universe resolver

    Example:

    ```python
    load("@rules_rust//rust:repositories.bzl", "load_arbitrary_tool")
    load("@rules_rust//rust/platform:triple.bzl", "get_host_triple")

    def _impl(repository_ctx):
        host_triple = get_host_triple(repository_ctx)

        load_arbitrary_tool(
            ctx = repository_ctx,
            tool_name = "cargo",
            tool_subdirectories = ["cargo"],
            target_triple = host_triple.str,
        )

    example = repository_rule(implementation = _impl)
    ```

    Args:
        repository_ctx (repository_ctx): The repository_rule's context object
        abi (str): Since there's no consistent way to check for ABI, this info
            may be explicitly provided

    Returns:
        struct: A triple struct; see the `triple` function in this module
    """

    # Detect the host's cpu architecture

    supported_architectures = {
        "linux": ["aarch64", "x86_64", "s390x", "powerpc64le"],
        "macos": ["aarch64", "x86_64"],
        "windows": ["aarch64", "x86_64"],
    }

    arch = repository_ctx.os.arch
    if arch == "amd64":
        arch = "x86_64"

    if arch == "ppc64le":
        arch = "powerpc64le"

    if "linux" in repository_ctx.os.name:
        _validate_cpu_architecture(arch, supported_architectures["linux"])
        return triple("{}-unknown-linux-{}".format(
            arch,
            abi or "gnu",
        ))

    if "mac" in repository_ctx.os.name:
        _validate_cpu_architecture(arch, supported_architectures["macos"])
        return triple("{}-apple-darwin".format(arch))

    if "win" in repository_ctx.os.name:
        _validate_cpu_architecture(arch, supported_architectures["windows"])
        return triple("{}-pc-windows-{}".format(
            arch,
            abi or "msvc",
        ))

    fail("Unhandled host os: {}", repository_ctx.os.name)
