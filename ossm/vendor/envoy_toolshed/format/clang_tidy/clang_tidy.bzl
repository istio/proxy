load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

TARGET_EXTENSIONS = [
    "c", "cc", "cpp", "cxx", "c++", "C", "h", "hh", "hpp", "hxx", "inc", "inl", "H",
]

def _run_tidy(
        ctx,
        wrapper,
        exe,
        additional_deps,
        config,
        flags,
        compilation_context,
        infile,
        discriminator):
    direct_deps = [infile, config]
    if exe.files_to_run.executable:
        direct_deps.append(exe.files_to_run.executable)
    if additional_deps.files:
        direct_deps.append(additional_deps.files)
    inputs = depset(
        direct = direct_deps,
        transitive = [compilation_context.headers],
    )

    args = ctx.actions.args()

    # specify the output file - twice
    outfile = ctx.actions.declare_file(
        "bazel_clang_tidy_%s.%s.clang-tidy.yaml" % (infile.path, discriminator)
    )

    args.add(exe.files_to_run.executable if exe.files else "clang-tidy")
    args.add(outfile.path)
    args.add(config.path)
    args.add("--export-fixes", outfile.path)
    args.add("--quiet")
    # add source to check
    args.add(infile.path)

    # start args passed to the compiler
    args.add("--")
    # add args specified by the toolchain, on the command line and rule copts
    args.add_all(flags)
    # add defines
    args.add_all(compilation_context.defines, before_each = "-D")
    args.add_all(compilation_context.local_defines, before_each = "-D")
    # add includes
    args.add_all(compilation_context.framework_includes, before_each = "-F")
    args.add_all(compilation_context.includes, before_each = "-I")
    args.add_all(compilation_context.quote_includes, before_each = "-iquote")
    args.add_all(compilation_context.system_includes, before_each = "-isystem")

    ctx.actions.run(
        inputs = inputs,
        outputs = [outfile],
        executable = wrapper,
        arguments = [args],
        mnemonic = "ClangTidy",
        use_default_shell_env = True,
        progress_message = "Run clang-tidy on {}".format(infile.short_path),
    )
    return outfile

def _rule_sources(ctx):
    srcs = []
    if hasattr(ctx.rule.attr, "srcs"):
        for src in ctx.rule.attr.srcs:
            srcs += [_src for _src in src.files.to_list() if _src.is_source and _src.extension in TARGET_EXTENSIONS]
    return srcs

def _toolchain_flags(ctx, action_name = ACTION_NAMES.cpp_compile):
    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
    )
    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = ctx.fragments.cpp.cxxopts + ctx.fragments.cpp.copts,
    )
    flags = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = action_name,
        variables = compile_variables,
    )
    return flags

def _safe_flags(flags):
    # Some flags might be used by GCC, but not understood by Clang.
    # Remove them here, to allow users to run clang-tidy, without having
    # a clang toolchain configured (that would produce a good command line with --compiler clang)
    unsupported_flags = [
        "-fno-canonical-system-headers",
        "-fstack-usage",
    ]
    return [flag for flag in flags if flag not in unsupported_flags and not flag.startswith("--sysroot")]

def _clang_tidy_aspect_impl(target, ctx):
    # if not a C/C++ target, we are not interested
    if not CcInfo in target:
        return []

    wrapper = ctx.attr._clang_tidy_wrapper.files_to_run
    exe = ctx.attr._clang_tidy_executable
    additional_deps = ctx.attr._clang_tidy_additional_deps
    config = ctx.attr._clang_tidy_config.files.to_list()[0]
    compilation_context = target[CcInfo].compilation_context

    rule_flags = ctx.rule.attr.copts if hasattr(ctx.rule.attr, "copts") else []
    safe_flags = {
        ACTION_NAMES.cpp_compile: _safe_flags(_toolchain_flags(ctx, ACTION_NAMES.cpp_compile) + rule_flags),
        ACTION_NAMES.c_compile: _safe_flags(_toolchain_flags(ctx, ACTION_NAMES.c_compile) + rule_flags),
    }
    outputs = [
        _run_tidy(
            ctx,
            wrapper,
            exe,
            additional_deps,
            config,
            safe_flags[ACTION_NAMES.c_compile if src.extension == "c" else ACTION_NAMES.cpp_compile],
            compilation_context,
            src,
            target.label.name,
        )
        for src in _rule_sources(ctx)
    ]

    return [
        OutputGroupInfo(report = depset(direct = outputs))
    ]

clang_tidy_aspect = aspect(
    implementation = _clang_tidy_aspect_impl,
    fragments = ["cpp"],
    attrs = {
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "_clang_tidy_wrapper": attr.label(default = Label("@envoy_toolshed//format/clang_tidy")),
        "_clang_tidy_executable": attr.label(default = Label("@envoy_toolshed//format/clang_tidy:executable")),
        "_clang_tidy_additional_deps": attr.label(default = Label("@envoy_toolshed//format/clang_tidy:additional_deps")),
        "_clang_tidy_config": attr.label(default = Label("@envoy_toolshed//format/clang_tidy:config")),
    },
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)
