"""MDBook toolchain implementations"""

def _mdbook_toolchain_impl(ctx):
    make_variable_info = platform_common.TemplateVariableInfo({
        "MDBOOK": ctx.file.mdbook.path,
    })

    all_files = [
        ctx.attr.mdbook[DefaultInfo].files,
        ctx.attr.mdbook[DefaultInfo].default_runfiles.files,
    ]

    for target in ctx.attr.plugins:
        all_files.append(target[DefaultInfo].files)
        all_files.append(target[DefaultInfo].default_runfiles.files)

    toolchain = platform_common.ToolchainInfo(
        make_variables = make_variable_info,
        mdbook = ctx.file.mdbook,
        plugins = depset(ctx.files.plugins),
        all_files = depset(transitive = all_files),
    )

    return [
        toolchain,
        make_variable_info,
    ]

mdbook_toolchain = rule(
    implementation = _mdbook_toolchain_impl,
    doc = "A [mdBook](https://rust-lang.github.io/mdBook/) toolchain.",
    attrs = {
        "mdbook": attr.label(
            doc = "A `mdBook` binary.",
            mandatory = True,
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
        "plugins": attr.label_list(
            doc = (
                "Executables to inject into `PATH` for use in " +
                "[preprocessor commands](https://rust-lang.github.io/mdBook/format/configuration/preprocessors.html#provide-your-own-command)."
            ),
            allow_files = True,
            cfg = "exec",
        ),
    },
)

def _current_mdbook_toolchain_impl(ctx):
    toolchain = ctx.toolchains["@rules_rust_mdbook//:toolchain_type"]
    return [
        DefaultInfo(
            files = depset([toolchain.mdbook]),
            runfiles = ctx.runfiles(files = [toolchain.mdbook]),
        ),
        toolchain,
        toolchain.make_variables,
    ]

current_mdbook_toolchain = rule(
    doc = "Access the current `mdbook_toolchain`.",
    implementation = _current_mdbook_toolchain_impl,
    toolchains = ["@rules_rust_mdbook//:toolchain_type"],
)

def _current_mdbook_binary_impl(ctx):
    toolchain = ctx.toolchains["@rules_rust_mdbook//:toolchain_type"]

    ext = ""
    if toolchain.mdbook.basename.endswith(".exe"):
        ext = ".exe"

    bin = ctx.actions.declare_file("{}{}".format(ctx.label.name, ext))
    ctx.actions.symlink(
        output = bin,
        target_file = toolchain.mdbook,
        is_executable = True,
    )

    return [
        DefaultInfo(
            files = depset([bin]),
            runfiles = ctx.runfiles(files = [toolchain.mdbook]),
            executable = bin,
        ),
    ]

current_mdbook_binary = rule(
    doc = "Access the current `mdbook_toolchain.mdbook` executable.",
    implementation = _current_mdbook_binary_impl,
    toolchains = ["@rules_rust_mdbook//:toolchain_type"],
    executable = True,
)

_BUILD_CONTENT = """\
load("@rules_rust_mdbook//:defs.bzl", "mdbook_toolchain")

mdbook_toolchain(
    name = "mdbook_toolchain",
    mdbook = "{mdbook}",
)

toolchain(
    name = "toolchain",
    exec_compatible_with = {exec_constraint_sets_serialized},
    toolchain = ":mdbook_toolchain",
    toolchain_type = "@rules_rust_mdbook//:toolchain_type",
    visibility = ["//visibility:public"],
)
"""

def _mdbook_toolchain_repository_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repository_ctx.file("BUILD.bazel", _BUILD_CONTENT.format(
        mdbook = str(Label(repository_ctx.attr.mdbook)),
        exec_constraint_sets_serialized = repr(repository_ctx.attr.exec_compatible_with),
    ))

mdbook_toolchain_repository = repository_rule(
    implementation = _mdbook_toolchain_repository_impl,
    doc = "A repository rule for defining an mdbook toolchain.",
    attrs = {
        "exec_compatible_with": attr.string_list(
            doc = "A list of constraints for the execution platform for this toolchain.",
        ),
        "mdbook": attr.string(
            doc = "The label to the mdbook executable.",
            mandatory = True,
        ),
    },
)
