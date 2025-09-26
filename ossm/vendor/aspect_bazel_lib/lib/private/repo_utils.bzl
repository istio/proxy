"""Utility functions for repository rules"""

def _is_darwin(rctx):
    """Returns true if the host operating system is Darwin"""
    return rctx.os.name.lower().startswith("mac os")

def _is_linux(rctx):
    """Returns true if the host operating system is Linux"""
    return rctx.os.name.lower().startswith("linux")

def _is_freebsd(rctx):
    """Returns true if the host operating system is FreeBSD"""
    return rctx.os.name.lower().startswith("freebsd")

def _is_windows(rctx):
    """Returns true if the host operating system is Windows"""
    return rctx.os.name.lower().find("windows") != -1

def _os(rctx):
    """Returns the name of the host operating system

    Args:
        rctx: rctx

    Returns:
        The string "windows", "linux", "freebsd" or "darwin" that describes the host os
    """
    if _is_darwin(rctx):
        return "darwin"
    if _is_linux(rctx):
        return "linux"
    if _is_freebsd(rctx):
        return "freebsd"
    if _is_windows(rctx):
        return "windows"
    fail("unrecognized os")

def _get_env_var(rctx, name, default):
    """Find an environment variable in system. Doesn't %-escape the value!

    Args:
        rctx: rctx
        name: environment variable name
        default: default value to return if env var is not set in system

    Returns:
        The environment variable value or the default if it is not set
    """

    # On Windows, the HOME environment variable is named differently.
    # See https://en.wikipedia.org/wiki/Home_directory#Default_home_directory_per_operating_system
    if name == "HOME" and _is_windows(rctx):
        name = "USERPROFILE"
    if name in rctx.os.environ:
        return rctx.os.environ[name]
    return default

def _get_home_directory(rctx):
    return _get_env_var(rctx, "HOME", None)

def _platform(rctx):
    """Returns a normalized name of the host os and CPU architecture.

    Alias archictures names are normalized:

    x86_64 => amd64
    aarch64 => arm64

    The result can be used to generate repository names for host toolchain
    repositories for toolchains that use these normalized names.

    Common os & architecture pairs that are returned are,

    - darwin_amd64
    - darwin_arm64
    - linux_amd64
    - linux_arm64
    - linux_s390x
    - linux_ppc64le
    - windows_amd64

    Args:
        rctx: rctx

    Returns:
        The normalized "<os>_<arch>" string of the host os and CPU architecture.
    """
    os = _os(rctx)

    # NB: in bazel 5.1.1 rctx.os.arch was added which https://github.com/bazelbuild/bazel/commit/32d1606dac2fea730abe174c41870b7ee70ae041.
    # Once we drop support for anything older than Bazel 5.1.1 than we can simplify
    # this function.
    if os == "windows":
        proc_arch = (_get_env_var(rctx, "PROCESSOR_ARCHITECTURE", "") or
                     _get_env_var(rctx, "PROCESSOR_ARCHITEW6432", ""))
        if proc_arch == "ARM64":
            arch = "arm64"
        else:
            arch = "amd64"
    else:
        arch = rctx.execute(["uname", "-m"]).stdout.strip()
    arch_map = {
        "x86_64": "amd64",
        "aarch64": "arm64",
    }
    if arch in arch_map.keys():
        arch = arch_map[arch]
    return "%s_%s" % (os, arch)

repo_utils = struct(
    is_darwin = _is_darwin,
    is_linux = _is_linux,
    is_windows = _is_windows,
    get_env_var = _get_env_var,
    get_home_directory = _get_home_directory,
    os = _os,
    platform = _platform,
)
