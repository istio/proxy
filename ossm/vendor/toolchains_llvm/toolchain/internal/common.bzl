# Copyright 2021 The Bazel Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

SUPPORTED_TARGETS = [
    ("linux", "x86_64"),
    ("linux", "aarch64"),
    ("linux", "armv7"),
    ("linux", "riscv64"),
    ("darwin", "x86_64"),
    ("darwin", "aarch64"),
    ("none", "riscv32"),
    ("none", "wasm32"),
    ("none", "wasm64"),
    ("none", "x86_64"),
    ("wasip1", "wasm32"),
    ("wasip1", "wasm64"),
]

# These are targets that can build without a sysroot.
SUPPORTED_NO_SYSROOT_TARGETS = [
    ("none", "riscv32"),
    ("none", "x86_64"),
]

# Map of tool name to its symlinked name in the tools directory.
# See tool_paths in toolchain/cc_toolchain_config.bzl.
_toolchain_tools = {
    name: name
    for name in [
        "clang-cpp",
        "clang-format",
        "clang-tidy",
        "clangd",
        "ld.lld",
        "llvm-ar",
        "llvm-dwp",
        "llvm-profdata",
        "llvm-cov",
        "llvm-nm",
        "llvm-objcopy",
        "llvm-objdump",
        "llvm-strip",
    ]
}

# Extra tools for Darwin.
_toolchain_tools_darwin = {
    # rules_foreign_cc relies on the filename of the linker to set flags.
    # Also see archive_flags in cc_toolchain_config.bzl.
    # https://github.com/bazelbuild/rules_foreign_cc/blob/5547abc63b12c521113208eea0c5d7f66ba494d4/foreign_cc/built_tools/make_build.bzl#L71
    # https://github.com/bazelbuild/rules_foreign_cc/blob/5547abc63b12c521113208eea0c5d7f66ba494d4/foreign_cc/private/cmake_script.bzl#L319
    "llvm-libtool-darwin": "libtool",
}

def exec_os_key(rctx):
    info = host_info(rctx)
    if info.dist.version == "":
        return "%s-%s" % (info.os, info.arch)
    else:
        return "%s-%s-%s" % (info.dist.name, info.dist.version, info.arch)

_known_distros = [
    # keep sorted
    "almalinux",
    "amzn",
    "arch",
    "centos",
    "debian",
    "fedora",
    "freebsd",
    "manjaro",
    "ol",
    "pop",
    "raspbian",
    "rhel",
    "suse",
    "ubuntu",
]

def _linux_dist(rctx):
    info = {}
    for line in rctx.read("/etc/os-release").splitlines():
        parts = line.split("=", 1)
        if len(parts) == 1:
            continue
        info[parts[0]] = parts[1]

    distname = info["ID"].strip('\"')

    if distname not in _known_distros and "ID_LIKE" in info:
        for distro in info["ID_LIKE"].strip('\"').split(" "):
            if distro in _known_distros:
                distname = distro
                break

    version = ""
    if "VERSION_ID" in info:
        version = info["VERSION_ID"].strip('"')
    elif "VERSION_CODENAME" in info:
        version = info["VERSION_CODENAME"].strip('"')

    return distname, version

def host_info(rctx):
    _os = os(rctx)
    _arch = arch(rctx)

    if _os == "linux" and not rctx.attr.exec_os:
        dist_name, dist_version = _linux_dist(rctx)
    else:
        dist_name = _os
        dist_version = ""
    return struct(
        arch = _arch,
        dist = struct(
            name = dist_name,
            version = dist_version,
        ),
        os = _os,
    )

def os(rctx):
    # Less granular host OS name, e.g. linux.

    name = rctx.attr.exec_os
    if name:
        if name in ("linux", "darwin", "none"):
            return name
        else:
            fail("Unsupported value for exec_os: %s" % name)
    return os_from_rctx(rctx)

def os_from_rctx(rctx):
    name = rctx.os.name
    if name == "linux":
        return "linux"
    elif name == "mac os x":
        return "darwin"
    elif name.startswith("windows"):
        return "windows"
    fail("Unsupported OS: " + name)

def os_bzl(os):
    # Return the OS string as used in bazel platform constraints.
    return {"darwin": "osx", "linux": "linux", "none": "none", "wasip1": "wasi"}[os]

def arch(rctx):
    arch = rctx.attr.exec_arch
    if arch:
        if arch in ("arm64", "aarch64"):
            return "aarch64"
        elif arch in ("amd64", "x86_64"):
            return "x86_64"
        else:
            fail("Unsupported value for exec_arch: %s" % arch)
    return arch_from_rctx(rctx)

def arch_from_rctx(rctx):
    arch = rctx.os.arch
    if arch == "arm64":
        return "aarch64"
    if arch == "amd64":
        return "x86_64"
    return arch

def is_standalone_arch(os, arch):
    return os == "none" and arch in ["wasm32", "wasm64"]

def os_arch_pair(os, arch):
    if is_standalone_arch(os, arch):
        return arch
    return "{}-{}".format(os, arch)

_supported_os_arch = [os_arch_pair(os, arch) for (os, arch) in SUPPORTED_TARGETS]

def supported_os_arch_keys():
    return _supported_os_arch

def check_os_arch_keys(keys):
    for k in keys:
        if k and k not in _supported_os_arch:
            fail("Unsupported {{os}}-{{arch}} key: {key}; valid keys are: {keys}".format(
                key = k,
                keys = ", ".join(_supported_os_arch),
            ))

def exec_os_arch_dict_value(rctx, attr_name, debug = False):
    # Gets a value from a dictionary keyed by host OS and arch.
    # Checks for the more specific key, then the less specific,
    # and finally the empty key as fallback.
    # Returns a tuple of the matching key and value.

    d = getattr(rctx.attr, attr_name)
    key1 = exec_os_key(rctx)
    if key1 in d:
        return (key1, d.get(key1))

    key2 = os_arch_pair(os(rctx), arch(rctx))
    if debug:
        print("`%s` attribute missing for key '%s' in repository '%s'; checking with key '%s'" % (attr_name, key1, rctx.name, key2))  # buildifier: disable=print
    if key2 in d:
        return (key2, d.get(key2))

    if debug:
        print("`%s` attribute missing for key '%s' in repository '%s'; checking with key ''" % (attr_name, key2, rctx.name))  # buildifier: disable=print
    return ("", d.get(""))  # Fallback to empty key.

def canonical_dir_path(path):
    if not path.endswith("/"):
        return path + "/"
    return path

def is_absolute_path(val):
    return val and val[0] == "/" and (len(val) == 1 or val[1] != "/")

def pkg_name_from_label(label):
    s = str(label)
    return s[:s.index(":")]

def pkg_path_from_label(label):
    if label.workspace_root:
        return label.workspace_root + "/" + label.package
    else:
        return label.package

def list_to_string(ls):
    if ls == None:
        return "None"
    return "[{}]".format(", ".join(["\"{}\"".format(d) for d in ls]))

def attr_dict(attr):
    # Returns a mutable dict of attr values from the struct. This is useful to
    # return updated attribute values as return values of repository_rule
    # implementations.

    tuples = []
    for key in dir(attr):
        if not hasattr(attr, key):
            fail("key %s not found in attributes" % key)
        if key[0] == "_":
            # Don't update private attrs.
            continue
        val = getattr(attr, key)

        # Make mutable copies of frozen types.
        typ = type(val)
        if typ == "dict":
            val = dict(val)
        elif typ == "list":
            val = list(val)
        elif typ == "builtin_function_or_method":
            # Functions can not be compared.
            continue

        tuples.append((key, val))

    return dict(tuples)

def toolchain_tools(os):
    tools = dict(_toolchain_tools)
    if os == "darwin":
        tools.update(_toolchain_tools_darwin)
    return tools
