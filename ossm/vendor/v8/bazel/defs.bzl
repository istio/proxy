# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FlagInfo = provider(fields = ["value"])

def _options_impl(ctx):
    return FlagInfo(value = ctx.build_setting_value)

_create_option_flag = rule(
    implementation = _options_impl,
    build_setting = config.bool(flag = True),
)

_create_option_string = rule(
    implementation = _options_impl,
    build_setting = config.string(flag = True),
)

_create_option_int = rule(
    implementation = _options_impl,
    build_setting = config.int(flag = True),
)

def v8_flag(name, default = False):
    _create_option_flag(name = name, build_setting_default = default)
    native.config_setting(name = "is_" + name, flag_values = {name: "True"})
    native.config_setting(name = "is_not_" + name, flag_values = {name: "False"})

def v8_string(name, default = ""):
    _create_option_string(name = name, build_setting_default = default)

def v8_int(name, default = 0):
    _create_option_int(name = name, build_setting_default = default)

def _custom_config_impl(ctx):
    defs = []
    defs.append("V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=" +
                str(ctx.attr._v8_typed_array_max_size_in_heap[FlagInfo].value))
    context = cc_common.create_compilation_context(defines = depset(defs))
    return [CcInfo(compilation_context = context)]

v8_custom_config = rule(
    implementation = _custom_config_impl,
    attrs = {
        "_v8_typed_array_max_size_in_heap": attr.label(default = ":v8_typed_array_max_size_in_heap"),
    },
)

def _config_impl(ctx):
    hdrs = []

    # Add headers
    for h in ctx.attr.hdrs:
        hdrs += h[DefaultInfo].files.to_list()
    defs = []

    # Add conditional_defines
    for f, d in ctx.attr.conditional_defines.items():
        if f[FlagInfo].value:
            defs.append(d)

    # Add defines
    for d in ctx.attr.defines:
        defs.append(d)
    context = cc_common.create_compilation_context(
        defines = depset(
            defs,
            transitive = [dep[CcInfo].compilation_context.defines for dep in ctx.attr.deps],
        ),
        headers = depset(
            hdrs,
            transitive = [dep[CcInfo].compilation_context.headers for dep in ctx.attr.deps],
        ),
    )
    return [CcInfo(compilation_context = context)]

v8_config = rule(
    implementation = _config_impl,
    attrs = {
        "conditional_defines": attr.label_keyed_string_dict(),
        "defines": attr.string_list(),
        "deps": attr.label_list(),
        "hdrs": attr.label_list(allow_files = True),
    },
)

def _default_args():
    return struct(
        deps = [":define_flags"],
        defines = select({
            "@v8//bazel/config:is_windows": [
                "UNICODE",
                "_UNICODE",
                "_CRT_RAND_S",
                "_WIN32_WINNT=0x0602",  # Override bazel default to Windows 8
            ],
            "//conditions:default": [],
        }),
        copts = select({
            "@v8//bazel/config:is_posix": [
                "-fPIC",
                "-fno-strict-aliasing",
                "-Werror",
                "-Wextra",
                "-Wno-unknown-warning-option",
                "-Wno-bitwise-instead-of-logical",
                "-Wno-builtin-assume-aligned-alignment",
                "-Wno-unused-parameter",
                "-Wno-implicit-int-float-conversion",
                "-Wno-deprecated-copy",
                "-Wno-non-virtual-dtor",
                "-isystem .",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_clang": [
                "-Wno-invalid-offsetof",
                "-Wno-unneeded-internal-declaration",
                "-std=c++17",
            ],
            "@v8//bazel/config:is_gcc": [
                "-Wno-extra",
                "-Wno-array-bounds",
                "-Wno-class-memaccess",
                "-Wno-comments",
                "-Wno-deprecated-declarations",
                "-Wno-implicit-fallthrough",
                "-Wno-int-in-bool-context",
                "-Wno-maybe-uninitialized",
                "-Wno-mismatched-new-delete",
                "-Wno-redundant-move",
                "-Wno-return-type",
                "-Wno-stringop-overflow",
                "-Wno-nonnull",
                "-Wno-pessimizing-move",
                # Use GNU dialect, because GCC doesn't allow using
                # ##__VA_ARGS__ when in standards-conforming mode.
                "-std=gnu++17",
            ],
            "@v8//bazel/config:is_windows": [
                "/std:c++17",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_gcc_fastbuild": [
                # Non-debug builds without optimizations fail because
                # of recursive inlining of "always_inline" functions.
                "-O1",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_clang_s390x": [
                "-fno-integrated-as",
            ],
            "//conditions:default": [],
        }) + select({
            "@envoy//bazel:no_debug_info": [
                "-g0",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_macos": [
                # The clang available on macOS catalina has a warning that isn't clean on v8 code.
                "-Wno-range-loop-analysis",

                # To supress warning on deprecated declaration on v8 code. For example:
                # external/v8/src/base/platform/platform-darwin.cc:56:22: 'getsectdatafromheader_64'
                # is deprecated: first deprecated in macOS 13.0.
                # https://bugs.chromium.org/p/v8/issues/detail?id=13428.
                "-Wno-deprecated-declarations",
            ],
            "//conditions:default": [],
        }),
        includes = ["include"],
        linkopts = select({
            "@v8//bazel/config:is_windows": [
                "Winmm.lib",
                "DbgHelp.lib",
                "Advapi32.lib",
            ],
            "@v8//bazel/config:is_macos": ["-pthread"],
            "//conditions:default": ["-Wl,--no-as-needed -ldl -pthread"],
        }) + select({
            ":should_add_rdynamic": ["-rdynamic"],
            "//conditions:default": [],
        }),
    )

ENABLE_I18N_SUPPORT_DEFINES = [
    "-DV8_INTL_SUPPORT",
    "-DICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC",
    # src/regexp/regexp-compiler-tonode.cc uses an unsafe ICU method and
    # access a character implicitly.
    "-DUNISTR_FROM_CHAR_EXPLICIT=",
]

def _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, icu_srcs, icu_deps):
    return noicu_srcs != [] or noicu_deps != [] or icu_srcs != [] or icu_deps != []

# buildifier: disable=function-docstring
def v8_binary(
        name,
        srcs,
        deps = [],
        includes = [],
        copts = [],
        linkopts = [],
        noicu_srcs = [],
        noicu_deps = [],
        icu_srcs = [],
        icu_deps = [],
        **kwargs):
    default = _default_args()
    if _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, icu_srcs, icu_deps):
        native.cc_binary(
            name = "noicu/" + name,
            srcs = srcs + noicu_srcs,
            deps = deps + noicu_deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )
        native.cc_binary(
            name = "icu/" + name,
            srcs = srcs + icu_srcs,
            deps = deps + icu_deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts + ENABLE_I18N_SUPPORT_DEFINES,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )
    else:
        native.cc_binary(
            name = name,
            srcs = srcs,
            deps = deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )

# buildifier: disable=function-docstring
def v8_library(
        name,
        srcs,
        deps = [],
        includes = [],
        copts = [],
        linkopts = [],
        noicu_srcs = [],
        noicu_deps = [],
        icu_srcs = [],
        icu_deps = [],
        **kwargs):
    default = _default_args()
    if _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, icu_srcs, icu_deps):
        native.cc_library(
            name = name + "_noicu",
            srcs = srcs + noicu_srcs,
            deps = deps + noicu_deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

        # Alias target used because of cc_library bug in bazel on windows
        # https://github.com/bazelbuild/bazel/issues/14237
        # TODO(victorgomes): Remove alias once bug is fixed
        native.alias(
            name = "noicu/" + name,
            actual = name + "_noicu",
        )
        native.cc_library(
            name = name + "_icu",
            srcs = srcs + icu_srcs,
            deps = deps + icu_deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts + ENABLE_I18N_SUPPORT_DEFINES,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

        # Alias target used because of cc_library bug in bazel on windows
        # https://github.com/bazelbuild/bazel/issues/14237
        # TODO(victorgomes): Remove alias once bug is fixed
        native.alias(
            name = "icu/" + name,
            actual = name + "_icu",
        )
    else:
        native.cc_library(
            name = name,
            srcs = srcs,
            deps = deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

def _torque_impl(ctx):
    if ctx.workspace_name == "v8":
        v8root = "."
    else:
        v8root = "external/v8"

    # Arguments
    args = []
    args += ctx.attr.args
    args.append("-o")
    args.append(ctx.bin_dir.path + "/" + v8root + "/" + ctx.attr.prefix + "/torque-generated")
    args.append("-strip-v8-root")
    args.append("-v8-root")
    args.append(v8root)

    # Sources
    args += [f.path for f in ctx.files.srcs]

    # Generate/declare output files
    outs = []
    for src in ctx.files.srcs:
        root, period, ext = src.path.rpartition(".")

        # Strip v8root
        if root[:len(v8root)] == v8root:
            root = root[len(v8root):]
        file = ctx.attr.prefix + "/torque-generated/" + root
        outs.append(ctx.actions.declare_file(file + "-tq-csa.cc"))
        outs.append(ctx.actions.declare_file(file + "-tq-csa.h"))
        outs.append(ctx.actions.declare_file(file + "-tq-inl.inc"))
        outs.append(ctx.actions.declare_file(file + "-tq.inc"))
        outs.append(ctx.actions.declare_file(file + "-tq.cc"))
    outs += [ctx.actions.declare_file(ctx.attr.prefix + "/torque-generated/" + f) for f in ctx.attr.extras]
    ctx.actions.run(
        outputs = outs,
        inputs = ctx.files.srcs,
        arguments = args,
        executable = ctx.executable.tool,
        mnemonic = "GenTorque",
        progress_message = "Generating Torque files",
    )
    return [DefaultInfo(files = depset(outs))]

_v8_torque = rule(
    implementation = _torque_impl,
    # cfg = v8_target_cpu_transition,
    attrs = {
        "prefix": attr.string(mandatory = True),
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "extras": attr.string_list(),
        "tool": attr.label(
            allow_files = True,
            executable = True,
            cfg = "exec",
        ),
        "args": attr.string_list(),
    },
)

def v8_torque(name, noicu_srcs, icu_srcs, args, extras):
    _v8_torque(
        name = "noicu/" + name,
        prefix = "noicu",
        srcs = noicu_srcs,
        args = args,
        extras = extras,
        tool = select({
            "@v8//bazel/config:v8_target_is_32_bits": ":torque_non_pointer_compression",
            "//conditions:default": ":torque",
        }),
    )
    _v8_torque(
        name = "icu/" + name,
        prefix = "icu",
        srcs = icu_srcs,
        args = args,
        extras = extras,
        tool = select({
            "@v8//bazel/config:v8_target_is_32_bits": ":torque_non_pointer_compression",
            "//conditions:default": ":torque",
        }),
    )

def _v8_target_cpu_transition_impl(settings, attr):
    # Check for an existing v8_target_cpu flag.
    if "@v8//bazel/config:v8_target_cpu" in settings:
        if settings["@v8//bazel/config:v8_target_cpu"] != "none":
            return

    # Auto-detect target architecture based on the --cpu flag.
    mapping = {
        "haswell": "x64",
        "k8": "x64",
        "x86_64": "x64",
        "darwin": "x64",
        "darwin_x86_64": "x64",
        "x64_windows": "x64",
        "x86": "ia32",
        "aarch64": "arm64",
        "arm64-v8a": "arm64",
        "arm": "arm64",
        "darwin_arm64": "arm64",
        "armeabi-v7a": "arm32",
        "s390x": "s390x",
        "riscv64": "riscv64",
        "ppc": "ppc64le",
    }
    v8_target_cpu = mapping[settings["//command_line_option:cpu"]]
    return {"@v8//bazel/config:v8_target_cpu": v8_target_cpu}

# Set the v8_target_cpu to be the correct architecture given the cpu specified
# on the command line.
v8_target_cpu_transition = transition(
    implementation = _v8_target_cpu_transition_impl,
    inputs = ["@v8//bazel/config:v8_target_cpu", "//command_line_option:cpu"],
    outputs = ["@v8//bazel/config:v8_target_cpu"],
)

def _mksnapshot(ctx):
    outs = [
        ctx.actions.declare_file(ctx.attr.prefix + "/snapshot.cc"),
        ctx.actions.declare_file(ctx.attr.prefix + "/embedded.S"),
    ]
    ctx.actions.run(
        outputs = outs,
        inputs = [],
        arguments = [
            "--embedded_variant=Default",
            "--startup_src",
            outs[0].path,
            "--embedded_src",
            outs[1].path,
        ] + ctx.attr.args,
        executable = ctx.executable.tool,
        progress_message = "Running mksnapshot",
    )
    return [DefaultInfo(files = depset(outs))]

_v8_mksnapshot = rule(
    implementation = _mksnapshot,
    attrs = {
        "args": attr.string_list(),
        "tool": attr.label(
            mandatory = True,
            allow_files = True,
            executable = True,
            cfg = "exec",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "prefix": attr.string(mandatory = True),
    },
    cfg = v8_target_cpu_transition,
)

def v8_mksnapshot(name, args):
    _v8_mksnapshot(
        name = "noicu/" + name,
        args = args,
        prefix = "noicu",
        tool = ":noicu/mksnapshot",
    )
    _v8_mksnapshot(
        name = "icu/" + name,
        args = args,
        prefix = "icu",
        tool = ":icu/mksnapshot",
    )

def _quote(val):
    if val[0] == '"' and val[-1] == '"':
        fail("String", val, "already quoted")
    return '"' + val + '"'

def _kv_bool_pair(k, v):
    return _quote(k) + ": " + v

def _json(kv_pairs):
    content = "{"
    for (k, v) in kv_pairs[:-1]:
        content += _kv_bool_pair(k, v) + ", "
    (k, v) = kv_pairs[-1]
    content += _kv_bool_pair(k, v)
    content += "}\n"
    return content

def build_config_content(cpu, icu):
    return _json([
        ("current_cpu", cpu),
        ("dcheck_always_on", "false"),
        ("is_android", "false"),
        ("is_asan", "false"),
        ("is_cfi", "false"),
        ("is_clang", "true"),
        ("is_component_build", "false"),
        ("is_debug", "false"),
        ("is_full_debug", "false"),
        ("is_gcov_coverage", "false"),
        ("is_msan", "false"),
        ("is_tsan", "false"),
        ("is_ubsan_vptr", "false"),
        ("target_cpu", cpu),
        ("v8_current_cpu", cpu),
        ("v8_dict_property_const_tracking", "false"),
        ("v8_enable_atomic_object_field_writes", "false"),
        ("v8_enable_concurrent_marking", "false"),
        ("v8_enable_i18n_support", icu),
        ("v8_enable_verify_predictable", "false"),
        ("v8_enable_verify_csa", "false"),
        ("v8_enable_lite_mode", "false"),
        ("v8_enable_runtime_call_stats", "false"),
        ("v8_enable_pointer_compression", "true"),
        ("v8_enable_pointer_compression_shared_cage", "false"),
        ("v8_enable_third_party_heap", "false"),
        ("v8_enable_webassembly", "false"),
        ("v8_control_flow_integrity", "false"),
        ("v8_enable_single_generation", "false"),
        ("v8_enable_sandbox", "false"),
        ("v8_enable_shared_ro_heap", "false"),
        ("v8_target_cpu", cpu),
    ])

# TODO(victorgomes): Create a rule (instead of a macro), that can
# dynamically populate the build config.
def v8_build_config(name):
    cpu = _quote("x64")
    native.genrule(
        name = "noicu/" + name,
        outs = ["noicu/" + name + ".json"],
        cmd = "echo '" + build_config_content(cpu, "false") + "' > \"$@\"",
    )
    native.genrule(
        name = "icu/" + name,
        outs = ["icu/" + name + ".json"],
        cmd = "echo '" + build_config_content(cpu, "true") + "' > \"$@\"",
    )
