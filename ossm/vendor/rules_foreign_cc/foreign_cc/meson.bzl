"""A rule for building projects using the [Meson](https://mesonbuild.com/) build system"""

load("@rules_cc//cc:defs.bzl", "CcInfo")
load("//foreign_cc:utils.bzl", "full_label")
load("//foreign_cc/built_tools:meson_build.bzl", "meson_tool")
load(
    "//foreign_cc/private:cc_toolchain_util.bzl",
    "absolutize_path_in_str",
    "get_flags_info",
    "get_tools_info",
)
load(
    "//foreign_cc/private:detect_root.bzl",
    "detect_root",
)
load(
    "//foreign_cc/private:framework.bzl",
    "CC_EXTERNAL_RULE_ATTRIBUTES",
    "CC_EXTERNAL_RULE_FRAGMENTS",
    "cc_external_rule_impl",
    "create_attrs",
    "expand_locations_and_make_variables",
)
load("//foreign_cc/private:make_script.bzl", "pkgconfig_script")
load("//foreign_cc/private:transitions.bzl", "foreign_cc_rule_variant")
load("//toolchains/native_tools:native_tools_toolchain.bzl", "native_tool_toolchain")
load("//toolchains/native_tools:tool_access.bzl", "get_cmake_data", "get_meson_data", "get_ninja_data", "get_pkgconfig_data")

def _meson_impl(ctx):
    """The implementation of the `meson` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers. See `cc_external_rule_impl`
    """

    meson_data = get_meson_data(ctx)
    cmake_data = get_cmake_data(ctx)
    ninja_data = get_ninja_data(ctx)
    pkg_config_data = get_pkgconfig_data(ctx)

    tools_data = [meson_data, cmake_data, ninja_data, pkg_config_data]

    attrs = create_attrs(
        ctx.attr,
        configure_name = "Meson",
        create_configure_script = _create_meson_script,
        tools_data = tools_data,
        meson_path = meson_data.path,
        cmake_path = cmake_data.path,
        ninja_path = ninja_data.path,
        pkg_config_path = pkg_config_data.path,
    )
    return cc_external_rule_impl(ctx, attrs)

def _create_meson_script(configureParameters):
    """Creates the bash commands for invoking commands to build meson projects

    Args:
        configureParameters (struct): See `ConfigureParameters`

    Returns:
        str: A string representing a section of a bash script
    """
    ctx = configureParameters.ctx
    attrs = configureParameters.attrs
    inputs = configureParameters.inputs

    tools = get_tools_info(ctx)
    script = pkgconfig_script(inputs.ext_build_dirs)

    # CFLAGS and CXXFLAGS are also set in foreign_cc/private/cmake_script.bzl, so that meson
    # can use the intended tools.
    # However, they are split by meson on whitespace. For Windows it's common to have spaces in path
    # https://github.com/mesonbuild/meson/issues/3565
    # Skip setting them in this case.
    if " " not in tools.cc:
        script.append("##export_var## CC {}".format(_absolutize(ctx.workspace_name, tools.cc)))
    if " " not in tools.cxx:
        script.append("##export_var## CXX {}".format(_absolutize(ctx.workspace_name, tools.cxx)))

    # set flags same as foreign_cc/private/cc_toolchain_util.bzl
    # cannot use get_flags_info() because bazel adds additional flags that
    # aren't compatible with compiler or linker above
    copts = (ctx.fragments.cpp.copts + ctx.fragments.cpp.conlyopts + getattr(ctx.attr, "copts", [])) or []
    cxxopts = (ctx.fragments.cpp.copts + ctx.fragments.cpp.cxxopts + getattr(ctx.attr, "copts", [])) or []

    if copts:
        script.append("##export_var## CFLAGS \"{} ${{CFLAGS:-}}\"".format(" ".join(copts).replace("\"", "'")))
    if cxxopts:
        script.append("##export_var## CXXFLAGS \"{} ${{CXXFLAGS:-}}\"".format(" ".join(cxxopts).replace("\"", "'")))

    flags = get_flags_info(ctx)
    if flags.cxx_linker_executable:
        script.append("##export_var## LDFLAGS \"{} ${{LDFLAGS:-}}\"".format(" ".join(flags.cxx_linker_executable).replace("\"", "'")))

    script.append("##export_var## CMAKE {}".format(attrs.cmake_path))
    script.append("##export_var## NINJA {}".format(attrs.ninja_path))
    script.append("##export_var## PKG_CONFIG {}".format(attrs.pkg_config_path))

    root = detect_root(ctx.attr.lib_source)
    data = ctx.attr.data + ctx.attr.build_data

    # Generate a list of arguments for meson
    options_str = " ".join([
        "-D{}=\"{}\"".format(key, ctx.attr.options[key])
        for key in ctx.attr.options
    ])

    prefix = "{} ".format(expand_locations_and_make_variables(ctx, attrs.tool_prefix, "tool_prefix", data)) if attrs.tool_prefix else ""

    setup_args_str = " ".join(expand_locations_and_make_variables(ctx, ctx.attr.setup_args, "setup_args", data))

    script.append("{prefix}{meson} setup --prefix={install_dir} {setup_args} {options} {source_dir}".format(
        prefix = prefix,
        meson = attrs.meson_path,
        install_dir = "$$INSTALLDIR$$",
        setup_args = setup_args_str,
        options = options_str,
        source_dir = "$$EXT_BUILD_ROOT$$/" + root,
    ))

    build_args = [] + ctx.attr.build_args
    build_args_str = " ".join([
        ctx.expand_location(arg, data)
        for arg in build_args
    ])

    script.append("{prefix}{meson} compile {args}".format(
        prefix = prefix,
        meson = attrs.meson_path,
        args = build_args_str,
    ))

    if ctx.attr.install:
        install_args = " ".join([
            ctx.expand_location(arg, data)
            for arg in ctx.attr.install_args
        ])
        script.append("{prefix}{meson} install {args}".format(
            prefix = prefix,
            meson = attrs.meson_path,
            args = install_args,
        ))

    return script

def _attrs():
    """Modifies the common set of attributes used by rules_foreign_cc and sets Meson specific attrs

    Returns:
        dict: Attributes of the `meson` rule
    """
    attrs = dict(CC_EXTERNAL_RULE_ATTRIBUTES)

    attrs.update({
        "build_args": attr.string_list(
            doc = "Arguments for the Meson build command",
            mandatory = False,
        ),
        "install": attr.bool(
            doc = "If True, the `meson install` comand will be performed after a build",
            default = True,
        ),
        "install_args": attr.string_list(
            doc = "Arguments for the meson install command",
            mandatory = False,
        ),
        "options": attr.string_dict(
            doc = (
                "Meson option entries to initialize (they will be passed with `-Dkey=value`)"
            ),
            mandatory = False,
            default = {},
        ),
        "setup_args": attr.string_list(
            doc = "Arguments for the Meson setup command",
            mandatory = False,
        ),
    })
    return attrs

meson = rule(
    doc = (
        "Rule for building external libraries with [Meson](https://mesonbuild.com/)."
    ),
    attrs = _attrs(),
    fragments = CC_EXTERNAL_RULE_FRAGMENTS,
    output_to_genfiles = True,
    provides = [CcInfo],
    implementation = _meson_impl,
    toolchains = [
        "@rules_foreign_cc//toolchains:meson_toolchain",
        "@rules_foreign_cc//toolchains:cmake_toolchain",
        "@rules_foreign_cc//toolchains:ninja_toolchain",
        "@rules_foreign_cc//toolchains:pkgconfig_toolchain",
        "@rules_foreign_cc//foreign_cc/private/framework:shell_toolchain",
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
)

def meson_with_requirements(name, requirements, **kwargs):
    """ Wrapper macro around Meson rule to add Python libraries required by the Meson build.

    Args:
        name: The target name
        requirements: List of Python "requirements", see https://github.com/bazelbuild/rules_python/tree/00545742ad2450863aeb82353d4275a1e5ed3f24#using-third_party-packages-as-dependencies
        **kwargs: Remaining keyword arguments
    """
    tags = kwargs.pop("tags", [])

    meson_tool(
        name = "meson_tool_for_{}".format(name),
        main = "@meson_src//:meson.py",
        data = ["@meson_src//:runtime"],
        requirements = requirements,
        tags = tags + ["manual"],
    )

    native_tool_toolchain(
        name = "built_meson_for_{}".format(name),
        env = {"MESON": "$(execpath :meson_tool_for_{})".format(name)},
        path = "$(execpath :meson_tool_for_{})".format(name),
        target = ":meson_tool_for_{}".format(name),
    )

    native.toolchain(
        name = "built_meson_toolchain_for_{}".format(name),
        toolchain = "built_meson_for_{}".format(name),
        toolchain_type = "@rules_foreign_cc//toolchains:meson_toolchain",
    )

    foreign_cc_rule_variant(
        name = name,
        rule = meson,
        toolchain = full_label("built_meson_toolchain_for_{}".format(name)),
        **kwargs
    )

def _absolutize(workspace_name, text, force = False):
    if text.strip(" ").startswith("C:") or text.strip(" ").startswith("c:"):
        return "\"{}\"".format(text)

    return absolutize_path_in_str(workspace_name, "$EXT_BUILD_ROOT/", text, force)
