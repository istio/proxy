"""Helpers for constructing supported Rust platform triples"""

load("//rust/platform:triple.bzl", "triple")

def _support(*, std = False, host_tools = False):
    """Identify the type of support an associated platform triple has.

    The source of truth is:
    https://doc.rust-lang.org/nightly/rustc/platform-support.html

    Args:
        std (bool, optional): Whether or not platform has a standard library artifact.
        host_tools (bool, optional): Whether or not platform has host tools artifacts.

    Returns:
        struct: The parameters above.
    """
    return struct(
        std = std,
        host_tools = host_tools,
    )

# All T1 Platforms should be supported, but aren't, see inline notes.
SUPPORTED_T1_PLATFORM_TRIPLES = {
    "aarch64-apple-darwin": _support(std = True, host_tools = True),
    "aarch64-unknown-linux-gnu": _support(std = True, host_tools = True),
    "aarch64-unknown-nixos-gnu": _support(std = True, host_tools = True),  # Same as `aarch64-unknown-linux-gnu` but with `@platforms//os:nixos`.
    "i686-apple-darwin": _support(std = True, host_tools = True),
    "i686-pc-windows-msvc": _support(std = True, host_tools = True),
    "i686-unknown-linux-gnu": _support(std = True, host_tools = True),
    "x86_64-apple-darwin": _support(std = True, host_tools = True),
    "x86_64-pc-windows-msvc": _support(std = True, host_tools = True),
    "x86_64-unknown-linux-gnu": _support(std = True, host_tools = True),
    "x86_64-unknown-nixos-gnu": _support(std = True, host_tools = True),  # Same as `x86_64-unknown-linux-gnu` but with `@platforms//os:nixos`.
    # N.B. These "alternative" envs are not supported, as bazel cannot distinguish between them
    # and others using existing @platforms// config_values
    #
    #"i686-pc-windows-gnu",
    #"x86_64-pc-windows-gnu",
}

# Some T2 Platforms are supported, provided we have mappings to `@platforms//...` entries.
# See `@rules_rust//rust/platform:triple_mappings.bzl` for the complete list.
SUPPORTED_T2_PLATFORM_TRIPLES = {
    "aarch64-apple-ios": _support(std = True, host_tools = False),
    "aarch64-apple-ios-sim": _support(std = True, host_tools = False),
    "aarch64-linux-android": _support(std = True, host_tools = False),
    "aarch64-pc-windows-msvc": _support(std = True, host_tools = True),
    "aarch64-unknown-fuchsia": _support(std = True, host_tools = False),
    "arm-unknown-linux-gnueabi": _support(std = True, host_tools = True),
    "armv7-linux-androideabi": _support(std = True, host_tools = False),
    "armv7-unknown-linux-gnueabi": _support(std = True, host_tools = True),
    "i686-linux-android": _support(std = True, host_tools = False),
    "i686-unknown-freebsd": _support(std = True, host_tools = False),
    "powerpc-unknown-linux-gnu": _support(std = True, host_tools = True),
    "riscv32imc-unknown-none-elf": _support(std = True, host_tools = False),
    "riscv64gc-unknown-none-elf": _support(std = True, host_tools = False),
    "s390x-unknown-linux-gnu": _support(std = True, host_tools = True),
    "thumbv7em-none-eabi": _support(std = True, host_tools = False),
    "thumbv8m.main-none-eabi": _support(std = True, host_tools = False),
    "wasm32-unknown-unknown": _support(std = True, host_tools = False),
    "wasm32-wasip1": _support(std = True, host_tools = False),
    "x86_64-apple-ios": _support(std = True, host_tools = False),
    "x86_64-linux-android": _support(std = True, host_tools = False),
    "x86_64-unknown-freebsd": _support(std = True, host_tools = True),
    "x86_64-unknown-fuchsia": _support(std = True, host_tools = False),
    "x86_64-unknown-none": _support(std = True, host_tools = False),
}

_T3_PLATFORM_TRIPLES = {
    "aarch64-unknown-nto-qnx710": _support(std = True, host_tools = False),
    "wasm64-unknown-unknown": _support(std = False, host_tools = False),
}

# The only T3 triples that are supported are ones with at least a stdlib
# artifact. However, it can be useful to know of additional triples so
# this list exists separate from the full list above.
SUPPORTED_T3_PLATFORM_TRIPLES = {
    triple: support
    for triple, support in _T3_PLATFORM_TRIPLES.items()
    if support.std
}

SUPPORTED_PLATFORM_TRIPLES = sorted(
    list(SUPPORTED_T1_PLATFORM_TRIPLES.keys()) +
    list(SUPPORTED_T2_PLATFORM_TRIPLES.keys()) +
    list(SUPPORTED_T3_PLATFORM_TRIPLES.keys()),
)

# Represents all platform triples `rules_rust` is configured to handle in some way.
# Note that with T3 platforms some artifacts may not be available which can lead to
# failures in the analysis phase. This list should be used sparingly.
ALL_PLATFORM_TRIPLES = (
    list(SUPPORTED_T1_PLATFORM_TRIPLES.keys()) +
    list(SUPPORTED_T2_PLATFORM_TRIPLES.keys()) +
    list(_T3_PLATFORM_TRIPLES.keys())
)

# CPUs that map to a `@platforms//cpu` entry
_CPU_ARCH_TO_BUILTIN_PLAT_SUFFIX = {
    "aarch64": "aarch64",
    "arm": "arm",
    "arm64e": "arm64e",
    "armv7": "armv7",
    "armv7s": None,
    "asmjs": None,
    "i386": "i386",
    "i586": None,
    "i686": "x86_32",
    "le32": None,
    "mips": None,
    "mipsel": None,
    "powerpc": "ppc",
    "powerpc64": None,
    "powerpc64le": "ppc64le",
    "riscv32": "riscv32",
    "riscv32imc": "riscv32",
    "riscv64": "riscv64",
    "riscv64gc": "riscv64",
    "s390": None,
    "s390x": "s390x",
    "thumbv6m": "armv6-m",
    "thumbv7em": "armv7e-m",
    "thumbv7m": "armv7-m",
    "thumbv8m.main": "armv8-m",
    "wasm32": "wasm32",
    "wasm64": "wasm64",
    "x86_64": "x86_64",
}

# Systems that map to a "@platforms//os entry
_SYSTEM_TO_BUILTIN_SYS_SUFFIX = {
    "android": "android",
    "bitrig": None,
    "darwin": "osx",
    "dragonfly": None,
    "eabi": "none",
    "eabihf": "none",
    "emscripten": None,
    "freebsd": "freebsd",
    "fuchsia": "fuchsia",
    "ios": "ios",
    "linux": "linux",
    "nacl": None,
    "netbsd": None,
    "nixos": "nixos",
    "none": "none",
    "nto": "qnx",
    "openbsd": "openbsd",
    "solaris": None,
    "unknown": None,
    "wasi": None,
    "wasip1": None,
    "windows": "windows",
}

_SYSTEM_TO_BINARY_EXT = {
    "android": "",
    "darwin": "",
    "eabi": "",
    "eabihf": "",
    "emscripten": ".js",
    "freebsd": "",
    "fuchsia": "",
    "ios": "",
    "linux": "",
    "nixos": "",
    "none": "",
    "nto": "",
    # This is currently a hack allowing us to have the proper
    # generated extension for the wasm target, similarly to the
    # windows target
    "unknown": ".wasm",
    "wasi": ".wasm",
    "wasip1": ".wasm",
    "windows": ".exe",
}

_SYSTEM_TO_STATICLIB_EXT = {
    "android": ".a",
    "darwin": ".a",
    "eabi": ".a",
    "eabihf": ".a",
    "emscripten": ".js",
    "freebsd": ".a",
    "fuchsia": ".a",
    "ios": ".a",
    "linux": ".a",
    "nixos": ".a",
    "none": ".a",
    "nto": ".a",
    "unknown": "",
    "wasi": "",
    "wasip1": "",
    "windows": ".lib",
}

_SYSTEM_TO_DYLIB_EXT = {
    "android": ".so",
    "darwin": ".dylib",
    "eabi": ".so",
    "eabihf": ".so",
    "emscripten": ".js",
    "freebsd": ".so",
    "fuchsia": ".so",
    "ios": ".dylib",
    "linux": ".so",
    "nixos": ".so",
    "none": ".so",
    "nto": ".a",
    "unknown": ".wasm",
    "wasi": ".wasm",
    "wasip1": ".wasm",
    "windows": ".dll",
}

# See https://github.com/rust-lang/rust/blob/master/src/libstd/build.rs
_SYSTEM_TO_STDLIB_LINKFLAGS = {
    # NOTE: Rust stdlib `build.rs` treats android as a subset of linux, rust rules treat android
    # as its own system.
    "android": ["-ldl", "-llog"],
    "bitrig": [],
    # TODO(gregbowyer): If rust stdlib is compiled for cloudabi with the backtrace feature it
    # includes `-lunwind` but this might not actually be required.
    # I am not sure which is the common configuration or how we encode it as a link flag.
    "cloudabi": ["-lunwind", "-lc", "-lcompiler_rt"],
    "darwin": ["-lSystem", "-lresolv"],
    "dragonfly": ["-lpthread"],
    "eabi": [],
    "eabihf": [],
    "emscripten": [],
    # TODO(bazelbuild/rules_cc#75):
    #
    # Right now bazel cc rules does not specify the exact flag setup needed for calling out system
    # libs, that is we dont know given a toolchain if it should be, for example,
    # `-lxxx` or `/Lxxx` or `xxx.lib` etc.
    #
    # We include the flag setup as they are _commonly seen_ on various platforms with a cc_rules
    # style override for people doing things like gnu-mingw on windows.
    #
    # If you are reading this ... sorry! set the env var `BAZEL_RUST_STDLIB_LINKFLAGS` to
    # what you need for your specific setup, for example like so
    # `BAZEL_RUST_STDLIB_LINKFLAGS="-ladvapi32:-lws2_32:-luserenv"`
    "freebsd": ["-lexecinfo", "-lpthread"],
    "fuchsia": ["-lzircon", "-lfdio"],
    "illumos": ["-lsocket", "-lposix4", "-lpthread", "-lresolv", "-lnsl", "-lumem"],
    "ios": ["-lSystem", "-lobjc", "-Wl,-framework,Security", "-Wl,-framework,Foundation", "-lresolv"],
    # TODO: This ignores musl. Longer term what does Bazel think about musl?
    "linux": ["-ldl", "-lpthread"],
    "nacl": [],
    "netbsd": ["-lpthread", "-lrt"],
    "nixos": ["-ldl", "-lpthread"],  # Same as `linux`.
    "none": [],
    "nto": [],
    "openbsd": ["-lpthread"],
    "solaris": ["-lsocket", "-lposix4", "-lpthread", "-lresolv"],
    "unknown": [],
    "uwp": ["ws2_32.lib"],
    "wasi": [],
    "wasip1": [],
    "windows": ["advapi32.lib", "ws2_32.lib", "userenv.lib", "Bcrypt.lib"],
}

def cpu_arch_to_constraints(cpu_arch, *, system = None):
    """Returns a list of contraint values which represents a triple's CPU.

    Args:
        cpu_arch (str): The architecture to match constraints for
        system (str, optional): The system for the associated ABI value.

    Returns:
        List: A list of labels to constraint values
    """
    if cpu_arch not in _CPU_ARCH_TO_BUILTIN_PLAT_SUFFIX:
        fail("CPU architecture \"{}\" is not supported by rules_rust".format(cpu_arch))

    plat_suffix = _CPU_ARCH_TO_BUILTIN_PLAT_SUFFIX[cpu_arch]

    # Patch armv7e-m to mf if hardfloat abi is selected
    if plat_suffix == "armv7e-m" and system == "eabihf":
        plat_suffix = "armv7e-mf"

    return ["@platforms//cpu:{}".format(plat_suffix)]

def vendor_to_constraints(_vendor):
    # TODO(acmcarther): Review:
    #
    # My current understanding is that vendors can't have a material impact on
    # constraint sets.
    return []

def system_to_constraints(system):
    if system not in _SYSTEM_TO_BUILTIN_SYS_SUFFIX:
        fail("System \"{}\" is not supported by rules_rust".format(system))

    sys_suffix = _SYSTEM_TO_BUILTIN_SYS_SUFFIX[system]

    return ["@platforms//os:{}".format(sys_suffix)]

def abi_to_constraints(abi, *, arch = None, system = None):
    """Return a list of constraint values which represents a triple's ABI.

    Note that some ABI values require additional info to accurately match a set of constraints.

    Args:
        abi (str): The abi value to match constraints for
        arch (str, optional): The architecture for the associated ABI value.
        system (str, optional): The system for the associated ABI value.

    Returns:
        List: A list of labels to constraint values
    """

    all_abi_constraints = []

    # add constraints for MUSL static compilation and linking
    # to separate the MUSL from the non-MUSL toolchain on x86_64
    # if abi == "musl" and system == "linux" and arch == "x86_64":
    # all_abi_constraints.append("//rust/platform/constraints:musl_on")

    # add constraints for iOS + watchOS simulator and device triples
    if system in ["ios", "watchos"]:
        if arch == "x86_64" or abi == "sim":
            all_abi_constraints.append("@build_bazel_apple_support//constraints:simulator")
        else:
            all_abi_constraints.append("@build_bazel_apple_support//constraints:device")

    # TODO(bazelbuild/platforms#38): Implement when C++ toolchain is more mature and we
    # figure out how they're doing this
    return all_abi_constraints

def triple_to_system(target_triple):
    """Returns a system name for a given platform triple

    **Deprecated**: Use triple() from triple.bzl directly.

    Args:
        target_triple (str): A platform triple. eg: `x86_64-unknown-linux-gnu`

    Returns:
        str: A system name
    """
    if type(target_triple) == "string":
        target_triple = triple(target_triple)
    return target_triple.system

def triple_to_arch(target_triple):
    """Returns a system architecture name for a given platform triple

    **Deprecated**: Use triple() from triple.bzl directly.

    Args:
        target_triple (str): A platform triple. eg: `x86_64-unknown-linux-gnu`

    Returns:
        str: A cpu architecture
    """
    if type(target_triple) == "string":
        target_triple = triple(target_triple)
    return target_triple.arch

def triple_to_abi(target_triple):
    """Returns a system abi name for a given platform triple

    **Deprecated**: Use triple() from triple.bzl directly.

    Args:
        target_triple (str): A platform triple. eg: `x86_64-unknown-linux-gnu`

    Returns:
        str: The triple's abi
    """
    if type(target_triple) == "string":
        target_triple = triple(target_triple)
    return target_triple.system

def system_to_dylib_ext(system):
    return _SYSTEM_TO_DYLIB_EXT[system]

def system_to_staticlib_ext(system):
    return _SYSTEM_TO_STATICLIB_EXT[system]

def system_to_binary_ext(system):
    return _SYSTEM_TO_BINARY_EXT[system]

def system_to_stdlib_linkflags(system):
    return _SYSTEM_TO_STDLIB_LINKFLAGS[system]

def triple_to_constraint_set(target_triple):
    """Returns a set of constraints for a given platform triple

    Args:
        target_triple (str): A platform triple. eg: `x86_64-unknown-linux-gnu`

    Returns:
        list: A list of constraints (each represented by a list of strings)
    """
    if target_triple in "wasm32-wasi":
        return [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
        ]
    if target_triple == "wasm32-wasip1":
        return [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
        ]
    if target_triple == "wasm32-unknown-unknown":
        return [
            "@platforms//cpu:wasm32",
            "@platforms//os:none",
        ]
    if target_triple == "wasm64-unknown-unknown":
        return [
            "@platforms//cpu:wasm64",
            "@platforms//os:none",
        ]

    triple_struct = triple(target_triple)

    constraint_set = []
    constraint_set += cpu_arch_to_constraints(
        triple_struct.arch,
        system = triple_struct.system,
    )
    constraint_set += vendor_to_constraints(triple_struct.vendor)
    constraint_set += system_to_constraints(triple_struct.system)
    constraint_set += abi_to_constraints(
        triple_struct.abi,
        arch = triple_struct.arch,
        system = triple_struct.system,
    )

    return constraint_set
