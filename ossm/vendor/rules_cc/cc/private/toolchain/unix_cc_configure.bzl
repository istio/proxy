# pylint: disable=g-bad-file-header
# Copyright 2016 The Bazel Authors. All rights reserved.
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
"""Configuring the C++ toolchain on Unix platforms."""

load(
    ":lib_cc_configure.bzl",
    "auto_configure_fail",
    "auto_configure_warning",
    "auto_configure_warning_maybe",
    "escape_string",
    "execute",
    "get_env_var",
    "get_starlark_list",
    "resolve_labels",
    "split_escaped",
    "which",
    "write_builtin_include_directory_paths",
)

def _uniq(iterable):
    """Remove duplicates from a list."""

    unique_elements = {element: None for element in iterable}
    return unique_elements.keys()

def _generate_system_module_map(repository_ctx, dirs, script_path):
    return execute(repository_ctx, [script_path] + dirs)

def _prepare_include_path(repo_ctx, path):
    """Resolve include path before outputting it into the crosstool.

    Args:
      repo_ctx: repository_ctx object.
      path: an include path to be resolved.

    Returns:
      Resolved include path. Resulting path is absolute if it is outside the
      repository and relative otherwise.
    """

    repo_root = str(repo_ctx.path("."))

    # We're on UNIX, so the path delimiter is '/'.
    repo_root += "/"
    path = str(repo_ctx.path(path))
    if path.startswith(repo_root):
        return path[len(repo_root):]
    return path

def _find_tool(repository_ctx, tool, overridden_tools):
    """Find a tool for repository, taking overridden tools into account."""
    if tool in overridden_tools:
        return overridden_tools[tool]
    return which(repository_ctx, tool, "/usr/bin/" + tool)

def _get_tool_paths(repository_ctx, overridden_tools):
    """Compute the %-escaped path to the various tools"""
    return dict({
        k: escape_string(_find_tool(repository_ctx, k, overridden_tools))
        for k in [
            "ar",
            "ld",
            "llvm-cov",
            "llvm-profdata",
            "cpp",
            "gcc",
            "dwp",
            "gcov",
            "nm",
            "objcopy",
            "objdump",
            "strip",
            "c++filt",
        ]
    }.items())

def _escaped_cplus_include_paths(repository_ctx):
    """Use ${CPLUS_INCLUDE_PATH} to compute the %-escaped list of flags for cxxflag."""
    if "CPLUS_INCLUDE_PATH" in repository_ctx.os.environ:
        result = []
        for p in repository_ctx.os.environ["CPLUS_INCLUDE_PATH"].split(":"):
            p = escape_string(str(repository_ctx.path(p)))  # Normalize the path
            result.append("-I" + p)
        return result
    else:
        return []

_INC_DIR_MARKER_BEGIN = "#include <...>"

# OSX add " (framework directory)" at the end of line, strip it.
_OSX_FRAMEWORK_SUFFIX = " (framework directory)"
_OSX_FRAMEWORK_SUFFIX_LEN = len(_OSX_FRAMEWORK_SUFFIX)

def _cxx_inc_convert(path):
    """Convert path returned by cc -E xc++ in a complete path. Doesn't %-escape the path!"""
    path = path.strip()
    if path.endswith(_OSX_FRAMEWORK_SUFFIX):
        path = path[:-_OSX_FRAMEWORK_SUFFIX_LEN].strip()
    return path

def _get_cxx_include_directories(repository_ctx, print_resource_dir_supported, cc, lang_flag, additional_flags = []):
    """Compute the list of C++ include directories."""
    result = repository_ctx.execute([cc, "-E", lang_flag, "-", "-v"] + additional_flags)
    index1 = result.stderr.find(_INC_DIR_MARKER_BEGIN)
    if index1 == -1:
        return []
    index1 = result.stderr.find("\n", index1)
    if index1 == -1:
        return []
    index2 = result.stderr.rfind("\n ")
    if index2 == -1 or index2 < index1:
        return []
    index2 = result.stderr.find("\n", index2 + 1)
    if index2 == -1:
        inc_dirs = result.stderr[index1 + 1:]
    else:
        inc_dirs = result.stderr[index1 + 1:index2].strip()

    inc_directories = [
        _prepare_include_path(repository_ctx, _cxx_inc_convert(p))
        for p in inc_dirs.split("\n")
    ]

    if print_resource_dir_supported:
        resource_dir = repository_ctx.execute(
            [cc, "-print-resource-dir"] + additional_flags,
        ).stdout.strip() + "/share"
        inc_directories.append(_prepare_include_path(repository_ctx, resource_dir))

    return inc_directories

def _is_compiler_option_supported(repository_ctx, cc, option):
    """Checks that `option` is supported by the C compiler. Doesn't %-escape the option."""
    result = repository_ctx.execute([
        cc,
        option,
        "-o",
        "/dev/null",
        "-c",
        str(repository_ctx.path("tools/cpp/empty.cc")),
    ])
    return result.stderr.find(option) == -1

def _is_linker_option_supported(repository_ctx, cc, force_linker_flags, option, pattern):
    """Checks that `option` is supported by the C linker. Doesn't %-escape the option."""
    result = repository_ctx.execute([cc] + force_linker_flags + [
        option,
        "-o",
        "/dev/null",
        str(repository_ctx.path("tools/cpp/empty.cc")),
    ])
    return result.stderr.find(pattern) == -1

def _find_linker_path(repository_ctx, cc, linker, is_clang):
    """Checks if a given linker is supported by the C compiler.

    Args:
      repository_ctx: repository_ctx.
      cc: path to the C compiler.
      linker: linker to find
      is_clang: whether the compiler is known to be clang

    Returns:
      String to put as value to -fuse-ld= flag, or None if linker couldn't be found.
    """
    result = repository_ctx.execute([
        cc,
        str(repository_ctx.path("tools/cpp/empty.cc")),
        "-o",
        "/dev/null",
        # Some macOS clang versions don't fail when setting -fuse-ld=gold, adding
        # these lines to force it to. This also means that we will not detect
        # gold when only a very old (year 2010 and older) is present.
        "-Wl,--start-lib",
        "-Wl,--end-lib",
        "-fuse-ld=" + linker,
        "-v",
    ])
    if result.return_code != 0:
        return None

    if not is_clang:
        return linker

    # Extract linker path from:
    # /usr/bin/clang ...
    # "/usr/bin/ld.lld" -pie -z ...
    linker_command = result.stderr.splitlines()[-1]
    return linker_command.strip().split(" ")[0].strip("\"'")

def _add_compiler_option_if_supported(repository_ctx, cc, option):
    """Returns `[option]` if supported, `[]` otherwise. Doesn't %-escape the option."""
    return [option] if _is_compiler_option_supported(repository_ctx, cc, option) else []

def _add_linker_option_if_supported(repository_ctx, cc, force_linker_flags, option, pattern):
    """Returns `[option]` if supported, `[]` otherwise. Doesn't %-escape the option."""
    return [option] if _is_linker_option_supported(repository_ctx, cc, force_linker_flags, option, pattern) else []

def _get_no_canonical_prefixes_opt(repository_ctx, cc):
    # If the compiler sometimes rewrites paths in the .d files without symlinks
    # (ie when they're shorter), it confuses Bazel's logic for verifying all
    # #included header files are listed as inputs to the action.

    # The '-fno-canonical-system-headers' should be enough, but clang does not
    # support it, so we also try '-no-canonical-prefixes' if first option does
    # not work.
    opt = _add_compiler_option_if_supported(
        repository_ctx,
        cc,
        "-fno-canonical-system-headers",
    )
    if len(opt) == 0:
        return _add_compiler_option_if_supported(
            repository_ctx,
            cc,
            "-no-canonical-prefixes",
        )
    return opt

def get_env(repository_ctx):
    """Convert the environment in a list of export if in Homebrew. Doesn't %-escape the result!

    Args:
      repository_ctx: The repository context.
    Returns:
      empty string or a list of exports in case we're running with homebrew. Don't ask me why.
    """
    env = repository_ctx.os.environ
    if "HOMEBREW_RUBY_PATH" in env:
        return "\n".join([
            "export %s='%s'" % (k, env[k].replace("'", "'\\''"))
            for k in env
            if k != "_" and k.find(".") == -1
        ])
    else:
        return ""

def _coverage_flags(repository_ctx, darwin):
    use_llvm_cov = "1" == get_env_var(
        repository_ctx,
        "BAZEL_USE_LLVM_NATIVE_COVERAGE",
        default = "0",
        enable_warning = False,
    )
    if darwin or use_llvm_cov:
        compile_flags = '"-fprofile-instr-generate",  "-fcoverage-mapping"'
        link_flags = '"-fprofile-instr-generate"'
    else:
        # gcc requires --coverage being passed for compilation and linking
        # https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#Instrumentation-Options
        compile_flags = '"--coverage"'
        link_flags = '"--coverage"'
    return compile_flags, link_flags

def _is_clang(repository_ctx, cc):
    return "clang" in repository_ctx.execute([cc, "-v"]).stderr

def _is_gcc(repository_ctx, cc):
    # GCC's version output uses the basename of argv[0] as the program name:
    # https://gcc.gnu.org/git/?p=gcc.git;a=blob;f=gcc/gcc.cc;h=158461167951c1b9540322fb19be6a89d6da07fc;hb=HEAD#l8728
    cc_stdout = repository_ctx.execute([cc, "--version"]).stdout
    return cc_stdout.startswith("gcc ") or cc_stdout.startswith("gcc-")

def _get_compiler_name(repository_ctx, cc):
    if _is_clang(repository_ctx, cc):
        return "clang"
    if _is_gcc(repository_ctx, cc):
        return "gcc"
    return "compiler"

def _find_generic(repository_ctx, name, env_name, overridden_tools, warn = False, silent = False):
    """Find a generic C++ toolchain tool. Doesn't %-escape the result."""

    if name in overridden_tools:
        return overridden_tools[name]

    result = name
    env_value = repository_ctx.os.environ.get(env_name)
    env_value_with_paren = ""
    if env_value != None:
        env_value = env_value.strip()
        if env_value:
            result = env_value
            env_value_with_paren = " (%s)" % env_value
    if result.startswith("/"):
        # Absolute path, maybe we should make this supported by our which function.
        return result
    result = repository_ctx.which(result)
    if result == None:
        msg = ("Cannot find %s or %s%s; either correct your path or set the %s" +
               " environment variable") % (name, env_name, env_value_with_paren, env_name)
        if warn:
            if not silent:
                auto_configure_warning(msg)
        else:
            auto_configure_fail(msg)
    return result

def find_cc(repository_ctx, overridden_tools):
    """Find the C compiler (gcc or clang) for the repository, considering overridden tools.

    Args:
      repository_ctx: The repository context.
      overridden_tools: A dictionary of overridden tools.

    Returns:
      The path to the C compiler.
    """
    cc = _find_generic(repository_ctx, "gcc", "CC", overridden_tools)
    if _is_clang(repository_ctx, cc):
        # If clang is run through a symlink with -no-canonical-prefixes, it does
        # not find its own include directory, which includes the headers for
        # libc++. Resolving the potential symlink here prevents this.
        result = repository_ctx.execute(["readlink", "-f", cc])
        if result.return_code == 0:
            return result.stdout.strip()
    return cc

def configure_unix_toolchain(repository_ctx, cpu_value, overridden_tools):
    """Configure C++ toolchain on Unix platforms.

    Args:
        repository_ctx: The repository context.
        cpu_value: The CPU value.
        overridden_tools: A dictionary of overridden tools.
    """
    paths = resolve_labels(repository_ctx, [
        "@rules_cc//cc/private/toolchain:BUILD.tpl",
        "@rules_cc//cc/private/toolchain:generate_system_module_map.sh",
        "@rules_cc//cc/private/toolchain:armeabi_cc_toolchain_config.bzl",
        "@rules_cc//cc/private/toolchain:unix_cc_toolchain_config.bzl",
        "@rules_cc//cc/private/toolchain:linux_cc_wrapper.sh.tpl",
        "@rules_cc//cc/private/toolchain:validate_static_library.sh.tpl",
        "@rules_cc//cc/private/toolchain:osx_cc_wrapper.sh.tpl",
        "@rules_cc//cc/private/toolchain:clang_deps_scanner_wrapper.sh.tpl",
        "@rules_cc//cc/private/toolchain:gcc_deps_scanner_wrapper.sh.tpl",
    ])

    repository_ctx.symlink(
        paths["@rules_cc//cc/private/toolchain:unix_cc_toolchain_config.bzl"],
        "cc_toolchain_config.bzl",
    )

    repository_ctx.symlink(
        paths["@rules_cc//cc/private/toolchain:armeabi_cc_toolchain_config.bzl"],
        "armeabi_cc_toolchain_config.bzl",
    )

    repository_ctx.file("tools/cpp/empty.cc", "int main() {}")
    darwin = cpu_value.startswith("darwin")
    bsd = cpu_value == "freebsd" or cpu_value == "openbsd"

    cc = find_cc(repository_ctx, overridden_tools)
    is_clang = _is_clang(repository_ctx, cc)
    overridden_tools = dict(overridden_tools)
    overridden_tools["gcc"] = cc
    overridden_tools["gcov"] = _find_generic(
        repository_ctx,
        "gcov",
        "GCOV",
        overridden_tools,
        warn = True,
        silent = True,
    )
    overridden_tools["llvm-cov"] = _find_generic(
        repository_ctx,
        "llvm-cov",
        "BAZEL_LLVM_COV",
        overridden_tools,
        warn = True,
        silent = True,
    )
    overridden_tools["llvm-profdata"] = _find_generic(
        repository_ctx,
        "llvm-profdata",
        "BAZEL_LLVM_PROFDATA",
        overridden_tools,
        warn = True,
        silent = True,
    )
    overridden_tools["ar"] = _find_generic(
        repository_ctx,
        "ar",
        "AR",
        overridden_tools,
        warn = True,
        silent = True,
    )
    if darwin:
        overridden_tools["gcc"] = "cc_wrapper.sh"
        overridden_tools["ar"] = _find_generic(repository_ctx, "libtool", "LIBTOOL", overridden_tools)

    auto_configure_warning_maybe(repository_ctx, "CC used: " + str(cc))
    tool_paths = _get_tool_paths(repository_ctx, overridden_tools)
    tool_paths["cpp-module-deps-scanner"] = "deps_scanner_wrapper.sh"

    # The parse_header tool needs to be a wrapper around the compiler as it has
    # to touch the output file.
    tool_paths["parse_headers"] = "cc_wrapper.sh"
    cc_toolchain_identifier = escape_string(get_env_var(
        repository_ctx,
        "CC_TOOLCHAIN_NAME",
        "local",
        False,
    ))

    if "nm" in tool_paths and "c++filt" in tool_paths:
        repository_ctx.template(
            "validate_static_library.sh",
            paths["@rules_cc//cc/private/toolchain:validate_static_library.sh.tpl"],
            {
                "%{c++filt}": escape_string(str(repository_ctx.path(tool_paths["c++filt"]))),
                # Certain weak symbols are otherwise listed with type T in the output of nm on macOS.
                "%{nm_extra_args}": "--no-weak" if darwin else "",
                "%{nm}": escape_string(str(repository_ctx.path(tool_paths["nm"]))),
            },
        )
        tool_paths["validate_static_library"] = "validate_static_library.sh"

    cc_wrapper_src = (
        "@rules_cc//cc/private/toolchain:osx_cc_wrapper.sh.tpl" if darwin else "@rules_cc//cc/private/toolchain:linux_cc_wrapper.sh.tpl"
    )
    repository_ctx.template(
        "cc_wrapper.sh",
        paths[cc_wrapper_src],
        {
            "%{cc}": escape_string(str(cc)),
            "%{env}": escape_string(get_env(repository_ctx)),
        },
    )
    deps_scanner_wrapper_src = (
        "@rules_cc//cc/private/toolchain:clang_deps_scanner_wrapper.sh.tpl" if is_clang else "@rules_cc//cc/private/toolchain:gcc_deps_scanner_wrapper.sh.tpl"
    )
    deps_scanner = "cpp-module-deps-scanner_not_found"
    if is_clang:
        cc_str = str(cc)
        path_arr = cc_str.split("/")[:-1]
        path_arr.append("clang-scan-deps")
        deps_scanner = "/".join(path_arr)
    repository_ctx.template(
        "deps_scanner_wrapper.sh",
        paths[deps_scanner_wrapper_src],
        {
            "%{cc}": escape_string(str(cc)),
            "%{deps_scanner}": escape_string(deps_scanner),
            "%{env}": escape_string(get_env(repository_ctx)),
        },
    )

    conly_opts = split_escaped(get_env_var(
        repository_ctx,
        "BAZEL_CONLYOPTS",
        "",
        False,
    ), ":")

    cxx_opts = split_escaped(get_env_var(
        repository_ctx,
        "BAZEL_CXXOPTS",
        "-std=c++17",
        False,
    ), ":")

    gold_or_lld_linker_path = (
        _find_linker_path(repository_ctx, cc, "lld", is_clang) or
        _find_linker_path(repository_ctx, cc, "gold", is_clang)
    )
    cc_path = repository_ctx.path(cc)
    if not str(cc_path).startswith(str(repository_ctx.path(".")) + "/"):
        # cc is outside the repository, set -B
        bin_search_flags = ["-B" + escape_string(str(cc_path.dirname))]
    else:
        # cc is inside the repository, don't set -B.
        bin_search_flags = []
    if not gold_or_lld_linker_path:
        ld_path = repository_ctx.path(tool_paths["ld"])
        if ld_path.dirname != cc_path.dirname:
            bin_search_flags.append("-B" + str(ld_path.dirname))
    force_linker_flags = []
    if gold_or_lld_linker_path:
        force_linker_flags.append("-fuse-ld=" + gold_or_lld_linker_path)

    # TODO: It's unclear why these flags aren't added on macOS.
    if bin_search_flags and not darwin:
        force_linker_flags.extend(bin_search_flags)
    use_libcpp = darwin or bsd
    is_as_needed_supported = _is_linker_option_supported(
        repository_ctx,
        cc,
        force_linker_flags,
        "-Wl,-no-as-needed",
        "-no-as-needed",
    )
    is_push_state_supported = _is_linker_option_supported(
        repository_ctx,
        cc,
        force_linker_flags,
        "-Wl,--push-state",
        "--push-state",
    )
    if use_libcpp:
        bazel_default_libs = ["-lc++", "-lm"]
    else:
        bazel_default_libs = ["-lstdc++", "-lm"]
    if is_as_needed_supported and is_push_state_supported:
        # Do not link against C++ standard libraries unless they are actually
        # used.
        # We assume that --push-state support implies --pop-state support.
        bazel_linklibs_elements = [
            arg
            for lib in bazel_default_libs
            for arg in ["-Wl,--push-state,-as-needed", lib, "-Wl,--pop-state"]
        ]
    else:
        bazel_linklibs_elements = bazel_default_libs
    bazel_linklibs = ":".join(bazel_linklibs_elements)
    bazel_linkopts = ""

    link_opts = split_escaped(get_env_var(
        repository_ctx,
        "BAZEL_LINKOPTS",
        bazel_linkopts,
        False,
    ), ":")
    link_libs = split_escaped(get_env_var(
        repository_ctx,
        "BAZEL_LINKLIBS",
        bazel_linklibs,
        False,
    ), ":")
    coverage_compile_flags, coverage_link_flags = _coverage_flags(repository_ctx, darwin)
    print_resource_dir_supported = _is_compiler_option_supported(
        repository_ctx,
        cc,
        "-print-resource-dir",
    )
    no_canonical_prefixes_opt = _get_no_canonical_prefixes_opt(repository_ctx, cc)
    builtin_include_directories = _uniq(
        _get_cxx_include_directories(repository_ctx, print_resource_dir_supported, cc, "-xc", conly_opts) +
        _get_cxx_include_directories(repository_ctx, print_resource_dir_supported, cc, "-xc++", cxx_opts) +
        _get_cxx_include_directories(
            repository_ctx,
            print_resource_dir_supported,
            cc,
            "-xc++",
            cxx_opts + ["-stdlib=libc++"],
        ) +
        _get_cxx_include_directories(
            repository_ctx,
            print_resource_dir_supported,
            cc,
            "-xc",
            no_canonical_prefixes_opt,
        ) +
        _get_cxx_include_directories(
            repository_ctx,
            print_resource_dir_supported,
            cc,
            "-xc++",
            cxx_opts + no_canonical_prefixes_opt,
        ) +
        _get_cxx_include_directories(
            repository_ctx,
            print_resource_dir_supported,
            cc,
            "-xc++",
            cxx_opts + no_canonical_prefixes_opt + ["-stdlib=libc++"],
        ) +
        # Always included in case the user has Xcode + the CLT installed, both
        # paths can be used interchangeably
        ["/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"],
    )

    generate_modulemap = is_clang
    if generate_modulemap:
        repository_ctx.file("module.modulemap", _generate_system_module_map(
            repository_ctx,
            builtin_include_directories,
            paths["@rules_cc//cc/private/toolchain:generate_system_module_map.sh"],
        ))
    extra_flags_per_feature = {}
    if is_clang:
        # Only supported by LLVM 14 and later, but required with C++20 and
        # layering_check as C++ modules are the default.
        # https://github.com/llvm/llvm-project/commit/0556138624edf48621dd49a463dbe12e7101f17d
        result = repository_ctx.execute([
            cc,
            "-Xclang",
            "-fno-cxx-modules",
            "-o",
            "/dev/null",
            "-c",
            str(repository_ctx.path("tools/cpp/empty.cc")),
        ])
        if "-fno-cxx-modules" not in result.stderr:
            extra_flags_per_feature["use_module_maps"] = ["-Xclang", "-fno-cxx-modules"]

    write_builtin_include_directory_paths(repository_ctx, cc, builtin_include_directories)
    repository_ctx.template(
        "BUILD",
        paths["@rules_cc//cc/private/toolchain:BUILD.tpl"],
        # @unsorted-dict-items
        {
            "%{abi_libc_version}": escape_string(get_env_var(
                repository_ctx,
                "ABI_LIBC_VERSION",
                "local",
                False,
            )),
            "%{abi_version}": escape_string(get_env_var(
                repository_ctx,
                "ABI_VERSION",
                "local",
                False,
            )),
            "%{cc_compiler_deps}": get_starlark_list([
                ":builtin_include_directory_paths",
                ":cc_wrapper",
                ":deps_scanner_wrapper",
            ] + (
                [":validate_static_library"] if "validate_static_library" in tool_paths else []
            )),
            "%{cc_toolchain_identifier}": cc_toolchain_identifier,
            "%{compile_flags}": get_starlark_list(
                [
                    "-fstack-protector",
                    # All warnings are enabled.
                    "-Wall",
                    # Enable a few more warnings that aren't part of -Wall.
                ] + ((
                    _add_compiler_option_if_supported(repository_ctx, cc, "-Wthread-safety") +
                    _add_compiler_option_if_supported(repository_ctx, cc, "-Wself-assign")
                )) + (
                    # Disable problematic warnings.
                    _add_compiler_option_if_supported(repository_ctx, cc, "-Wunused-but-set-parameter") +
                    # has false positives
                    _add_compiler_option_if_supported(repository_ctx, cc, "-Wno-free-nonheap-object") +
                    # Enable coloring even if there's no attached terminal. Bazel removes the
                    # escape sequences if --nocolor is specified.
                    _add_compiler_option_if_supported(repository_ctx, cc, "-fcolor-diagnostics")
                ) + [
                    # Keep stack frames for debugging, even in opt mode.
                    "-fno-omit-frame-pointer",
                ],
            ),
            "%{compiler}": escape_string(get_env_var(
                repository_ctx,
                "BAZEL_COMPILER",
                _get_compiler_name(repository_ctx, cc),
                False,
            )),
            "%{conly_flags}": get_starlark_list(conly_opts),
            "%{coverage_compile_flags}": coverage_compile_flags,
            "%{coverage_link_flags}": coverage_link_flags,
            "%{cxx_builtin_include_directories}": get_starlark_list(builtin_include_directories),
            "%{cxx_flags}": get_starlark_list(cxx_opts + _escaped_cplus_include_paths(repository_ctx)),
            "%{dbg_compile_flags}": get_starlark_list(["-g"]),
            "%{extra_flags_per_feature}": repr(extra_flags_per_feature),
            "%{host_system_name}": escape_string(get_env_var(
                repository_ctx,
                "BAZEL_HOST_SYSTEM",
                "local",
                False,
            )),
            "%{link_flags}": get_starlark_list(force_linker_flags + (
                ["-Wl,-no-as-needed"] if is_as_needed_supported else []
            ) + _add_linker_option_if_supported(
                repository_ctx,
                cc,
                force_linker_flags,
                "-Wl,-z,relro,-z,now",
                "-z",
            ) + (
                [
                    "-headerpad_max_install_names",
                ] if darwin else [
                    # Gold linker only? Can we enable this by default?
                    # "-Wl,--warn-execstack",
                    # "-Wl,--detect-odr-violations"
                ] + _add_compiler_option_if_supported(
                    # Have gcc return the exit code from ld.
                    repository_ctx,
                    cc,
                    "-pass-exit-codes",
                )
            ) + link_opts),
            "%{link_libs}": get_starlark_list(link_libs),
            "%{modulemap}": ("\":module.modulemap\"" if generate_modulemap else "None"),
            "%{name}": cpu_value,
            "%{opt_compile_flags}": get_starlark_list(
                [
                    # No debug symbols.
                    # Maybe we should enable https://gcc.gnu.org/wiki/DebugFission for opt or
                    # even generally? However, that can't happen here, as it requires special
                    # handling in Bazel.
                    "-g0",

                    # Conservative choice for -O
                    # -O3 can increase binary size and even slow down the resulting binaries.
                    # Profile first and / or use FDO if you need better performance than this.
                    "-O2",

                    # Security hardening on by default.
                    # Conservative choice; -D_FORTIFY_SOURCE=2 may be unsafe in some cases.
                    "-D_FORTIFY_SOURCE=1",

                    # Disable assertions
                    "-DNDEBUG",

                    # Removal of unused code and data at link time (can this increase binary
                    # size in some cases?).
                    "-ffunction-sections",
                    "-fdata-sections",
                ],
            ),
            "%{opt_link_flags}": get_starlark_list(
                ["-Wl,-dead_strip"] if darwin else _add_linker_option_if_supported(
                    repository_ctx,
                    cc,
                    force_linker_flags,
                    "-Wl,--gc-sections",
                    "-gc-sections",
                ),
            ),
            "%{supports_start_end_lib}": "True" if gold_or_lld_linker_path else "False",
            "%{target_cpu}": escape_string(get_env_var(
                repository_ctx,
                "BAZEL_TARGET_CPU",
                cpu_value,
                False,
            )),
            "%{target_libc}": "macosx" if darwin else escape_string(get_env_var(
                repository_ctx,
                "BAZEL_TARGET_LIBC",
                "local",
                False,
            )),
            "%{target_system_name}": escape_string(get_env_var(
                repository_ctx,
                "BAZEL_TARGET_SYSTEM",
                "local",
                False,
            )),
            "%{tool_paths}": ",\n        ".join(
                ['"%s": "%s"' % (k, v) for k, v in tool_paths.items() if v != None],
            ),
            "%{unfiltered_compile_flags}": get_starlark_list(
                _get_no_canonical_prefixes_opt(repository_ctx, cc) + [
                    # Make C++ compilation deterministic. Use linkstamping instead of these
                    # compiler symbols.
                    "-Wno-builtin-macro-redefined",
                    "-D__DATE__=\\\"redacted\\\"",
                    "-D__TIMESTAMP__=\\\"redacted\\\"",
                    "-D__TIME__=\\\"redacted\\\"",
                ],
            ),
        },
    )
