# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Implementation of sphinx rules."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//python:py_binary.bzl", "py_binary")
load("//python/private:util.bzl", "add_tag", "copy_propagating_kwargs")  # buildifier: disable=bzl-visibility
load(":sphinx_docs_library_info.bzl", "SphinxDocsLibraryInfo")

_SPHINX_BUILD_MAIN_SRC = Label("//sphinxdocs/private:sphinx_build.py")
_SPHINX_SERVE_MAIN_SRC = Label("//sphinxdocs/private:sphinx_server.py")

_SphinxSourceTreeInfo = provider(
    doc = "Information about source tree for Sphinx to build.",
    fields = {
        "source_dir_runfiles_path": """
:type: str

Runfiles-root relative path of the root directory for the source files.
""",
        "source_root": """
:type: str

Exec-root relative path of the root directory for the source files (which are in DefaultInfo.files)
""",
    },
)

_SphinxRunInfo = provider(
    doc = "Information for running the underlying Sphinx command directly",
    fields = {
        "per_format_args": """
:type: dict[str, struct]

A dict keyed by output format name. The values are a struct with attributes:
* args: a `list[str]` of args to run this format's build
* env: a `dict[str, str]` of environment variables to set for this format's build
""",
        "source_tree": """
:type: Target

Target with the source tree files
""",
        "sphinx": """
:type: Target

The sphinx-build binary to run.
""",
        "tools": """
:type: list[Target]

Additional tools Sphinx needs
""",
    },
)

def sphinx_build_binary(name, py_binary_rule = py_binary, **kwargs):
    """Create an executable with the sphinx-build command line interface.

    The `deps` must contain the sphinx library and any other extensions Sphinx
    needs at runtime.

    Args:
        name: {type}`str` name of the target. The name "sphinx-build" is the
            conventional name to match what Sphinx itself uses.
        py_binary_rule: {type}`callable` A `py_binary` compatible callable
            for creating the target. If not set, the regular `py_binary`
            rule is used. This allows using the version-aware rules, or
            other alternative implementations.
        **kwargs: {type}`dict` Additional kwargs to pass onto `py_binary`. The `srcs` and
            `main` attributes must not be specified.
    """
    add_tag(kwargs, "@rules_python//sphinxdocs:sphinx_build_binary")
    py_binary_rule(
        name = name,
        srcs = [_SPHINX_BUILD_MAIN_SRC],
        main = _SPHINX_BUILD_MAIN_SRC,
        **kwargs
    )

def sphinx_docs(
        name,
        *,
        srcs = [],
        deps = [],
        renamed_srcs = {},
        sphinx,
        config,
        formats,
        strip_prefix = "",
        extra_opts = [],
        tools = [],
        allow_persistent_workers = True,
        **kwargs):
    """Generate docs using Sphinx.

    Generates targets:
    * `<name>`: The output of this target is a directory for each
      format Sphinx creates. This target also has a separate output
      group for each format. e.g. `--output_group=html` will only build
      the "html" format files.
    * `<name>.serve`: A binary that locally serves the HTML output. This
      allows previewing docs during development.
    * `<name>.run`: A binary that directly runs the underlying Sphinx command
      to build the docs. This is a debugging aid.

    Args:
        name: {type}`Name` name of the docs rule.
        srcs: {type}`list[label]` The source files for Sphinx to process.
        deps: {type}`list[label]` of {obj}`sphinx_docs_library` targets.
        renamed_srcs: {type}`dict[label, dict]` Doc source files for Sphinx that
            are renamed. This is typically used for files elsewhere, such as top
            level files in the repo.
        sphinx: {type}`label` the Sphinx tool to use for building
            documentation. Because Sphinx supports various plugins, you must
            construct your own binary with the necessary dependencies. The
            {obj}`sphinx_build_binary` rule can be used to define such a binary, but
            any executable supporting the `sphinx-build` command line interface
            can be used (typically some `py_binary` program).
        config: {type}`label` the Sphinx config file (`conf.py`) to use.
        formats: (list of str) the formats (`-b` flag) to generate documentation
            in. Each format will become an output group.
        strip_prefix: {type}`str` A prefix to remove from the file paths of the
            source files. e.g., given `//docs:foo.md`, stripping `docs/` makes
            Sphinx see `foo.md` in its generated source directory. If not
            specified, then {any}`native.package_name` is used.
        extra_opts: {type}`list[str]` Additional options to pass onto Sphinx building.
            On each provided option, a location expansion is performed.
            See {any}`ctx.expand_location`.
        tools: {type}`list[label]` Additional tools that are used by Sphinx and its plugins.
            This just makes the tools available during Sphinx execution. To locate
            them, use {obj}`extra_opts` and `$(location)`.
        allow_persistent_workers: {type}`bool` (experimental) If true, allow
            using persistent workers for running Sphinx, if Bazel decides to do so.
            This can improve incremental building of docs.
        **kwargs: {type}`dict` Common attributes to pass onto rules.
    """
    add_tag(kwargs, "@rules_python//sphinxdocs:sphinx_docs")
    common_kwargs = copy_propagating_kwargs(kwargs)

    internal_name = "_{}".format(name.lstrip("_"))

    _sphinx_source_tree(
        name = internal_name + "/_sources",
        srcs = srcs,
        deps = deps,
        renamed_srcs = renamed_srcs,
        config = config,
        strip_prefix = strip_prefix,
        **common_kwargs
    )
    _sphinx_docs(
        name = name,
        sphinx = sphinx,
        formats = formats,
        source_tree = internal_name + "/_sources",
        extra_opts = extra_opts,
        tools = tools,
        allow_persistent_workers = allow_persistent_workers,
        **kwargs
    )

    html_name = internal_name + "_html"
    native.filegroup(
        name = html_name,
        srcs = [name],
        output_group = "html",
        **common_kwargs
    )

    common_kwargs_with_manual_tag = dict(common_kwargs)
    common_kwargs_with_manual_tag["tags"] = list(common_kwargs.get("tags") or []) + ["manual"]

    py_binary(
        name = name + ".serve",
        srcs = [_SPHINX_SERVE_MAIN_SRC],
        main = _SPHINX_SERVE_MAIN_SRC,
        data = [html_name],
        deps = [Label("//python/runfiles")],
        args = [
            "$(rlocationpath {})".format(html_name),
        ],
        **common_kwargs_with_manual_tag
    )
    sphinx_run(
        name = name + ".run",
        docs = name,
        **common_kwargs_with_manual_tag
    )

def _sphinx_docs_impl(ctx):
    source_tree_info = ctx.attr.source_tree[_SphinxSourceTreeInfo]
    source_dir_path = source_tree_info.source_root
    inputs = ctx.attr.source_tree[DefaultInfo].files

    per_format_args = {}
    outputs = {}
    for format in ctx.attr.formats:
        output_dir, args_env = _run_sphinx(
            ctx = ctx,
            format = format,
            source_path = source_dir_path,
            output_prefix = paths.join(ctx.label.name, "_build"),
            inputs = inputs,
            allow_persistent_workers = ctx.attr.allow_persistent_workers,
        )
        outputs[format] = output_dir
        per_format_args[format] = args_env
    return [
        DefaultInfo(files = depset(outputs.values())),
        OutputGroupInfo(**{
            format: depset([output])
            for format, output in outputs.items()
        }),
        _SphinxRunInfo(
            sphinx = ctx.attr.sphinx,
            source_tree = ctx.attr.source_tree,
            tools = ctx.attr.tools,
            per_format_args = per_format_args,
        ),
    ]

_sphinx_docs = rule(
    implementation = _sphinx_docs_impl,
    attrs = {
        "allow_persistent_workers": attr.bool(
            doc = "(experimental) Whether to invoke Sphinx as a persistent worker.",
            default = False,
        ),
        "extra_opts": attr.string_list(
            doc = "Additional options to pass onto Sphinx. These are added after " +
                  "other options, but before the source/output args.",
        ),
        "formats": attr.string_list(doc = "Output formats for Sphinx to create."),
        "source_tree": attr.label(
            doc = "Directory of files for Sphinx to process.",
            providers = [_SphinxSourceTreeInfo],
        ),
        "sphinx": attr.label(
            executable = True,
            cfg = "exec",
            mandatory = True,
            doc = "Sphinx binary to generate documentation.",
        ),
        "tools": attr.label_list(
            cfg = "exec",
            doc = "Additional tools that are used by Sphinx and its plugins.",
        ),
        "_extra_defines_flag": attr.label(default = "//sphinxdocs:extra_defines"),
        "_extra_env_flag": attr.label(default = "//sphinxdocs:extra_env"),
        "_quiet_flag": attr.label(default = "//sphinxdocs:quiet"),
    },
)

def _run_sphinx(ctx, format, source_path, inputs, output_prefix, allow_persistent_workers):
    output_dir = ctx.actions.declare_directory(paths.join(output_prefix, format))

    run_args = []  # Copy of the args to forward along to debug runner
    args = ctx.actions.args()  # Args passed to the action

    # An args file is required for persistent workers, but we don't know if
    # the action will use worker mode or not (settings we can't see may
    # force non-worker mode). For consistency, always use a params file.
    args.use_param_file("@%s", use_always = True)
    args.set_param_file_format("multiline")

    # NOTE: sphinx_build.py relies on the first two args being the srcdir and
    # outputdir, in that order.
    args.add(source_path)
    args.add(output_dir.path)

    args.add("--show-traceback")  # Full tracebacks on error
    run_args.append("--show-traceback")
    args.add(format, format = "--builder=%s")
    run_args.append("--builder={}".format(format))

    if ctx.attr._quiet_flag[BuildSettingInfo].value:
        # Not added to run_args because run_args is for debugging
        args.add("--quiet")  # Suppress stdout informational text

    # Build in parallel, if possible
    # Don't add to run_args: parallel building breaks interactive debugging
    args.add("--jobs=auto")

    # Put the doctree dir outside of the output directory.
    # This allows it to be reused between invocations when possible; Bazel
    # clears the output directory every action invocation.
    # * For workers, they can fully re-use it.
    # * For non-workers, it can be reused when sandboxing is disabled via
    #   the `no-sandbox` tag or execution requirement.
    #
    # We also use a non-dot prefixed name so it shows up more visibly.
    args.add(paths.join(output_dir.path + "_doctrees"), format = "--doctree-dir=%s")

    for opt in ctx.attr.extra_opts:
        expanded = ctx.expand_location(opt)
        args.add(expanded)
        run_args.append(expanded)

    extra_defines = ctx.attr._extra_defines_flag[_FlagInfo].value
    args.add_all(extra_defines, before_each = "--define")
    for define in extra_defines:
        run_args.extend(("--define", define))

    env = dict([
        v.split("=", 1)
        for v in ctx.attr._extra_env_flag[_FlagInfo].value
    ])

    tools = []
    for tool in ctx.attr.tools:
        tools.append(tool[DefaultInfo].files_to_run)

    # NOTE: Command line flags or RBE capabilities may override the execution
    # requirements and disable workers. Thus, we can't assume that these
    # exec requirements will actually be respected.
    execution_requirements = {}
    if allow_persistent_workers:
        execution_requirements["supports-workers"] = "1"
        execution_requirements["requires-worker-protocol"] = "json"

    ctx.actions.run(
        executable = ctx.executable.sphinx,
        arguments = [args],
        inputs = inputs,
        outputs = [output_dir],
        tools = tools,
        mnemonic = "SphinxBuildDocs",
        progress_message = "Sphinx building {} for %{{label}}".format(format),
        env = env,
        execution_requirements = execution_requirements,
    )
    return output_dir, struct(args = run_args, env = env)

def _sphinx_source_tree_impl(ctx):
    # Sphinx only accepts a single directory to read its doc sources from.
    # Because plain files and generated files are in different directories,
    # we need to merge the two into a single directory.
    source_prefix = ctx.label.name
    sphinx_source_files = []

    # Materialize a file under the `_sources` dir
    def _relocate(source_file, dest_path = None):
        if not dest_path:
            dest_path = source_file.short_path.removeprefix(ctx.attr.strip_prefix)

        dest_path = paths.join(source_prefix, dest_path)
        if source_file.is_directory:
            dest_file = ctx.actions.declare_directory(dest_path)
        else:
            dest_file = ctx.actions.declare_file(dest_path)
        ctx.actions.symlink(
            output = dest_file,
            target_file = source_file,
            progress_message = "Symlinking Sphinx source %{input} to %{output}",
        )
        sphinx_source_files.append(dest_file)
        return dest_file

    # Though Sphinx has a -c flag, we move the config file into the sources
    # directory to make the config more intuitive because some configuration
    # options are relative to the config location, not the sources directory.
    source_conf_file = _relocate(ctx.file.config)
    sphinx_source_dir_path = paths.dirname(source_conf_file.path)

    for src in ctx.attr.srcs:
        if SphinxDocsLibraryInfo in src:
            fail((
                "In attribute srcs: target {src} is misplaced here: " +
                "sphinx_docs_library targets belong in the deps attribute."
            ).format(src = src))

    for orig_file in ctx.files.srcs:
        _relocate(orig_file)

    for src_target, dest in ctx.attr.renamed_srcs.items():
        src_files = src_target[DefaultInfo].files.to_list()
        if len(src_files) != 1:
            fail("A single file must be specified to be renamed. Target {} " +
                 "generate {} files: {}".format(
                     src_target,
                     len(src_files),
                     src_files,
                 ))
        _relocate(src_files[0], dest)

    for t in ctx.attr.deps:
        info = t[SphinxDocsLibraryInfo]
        for entry in info.transitive.to_list():
            for original in entry.files:
                new_path = entry.prefix + original.short_path.removeprefix(entry.strip_prefix)
                _relocate(original, new_path)

    return [
        DefaultInfo(
            files = depset(sphinx_source_files),
        ),
        _SphinxSourceTreeInfo(
            source_root = sphinx_source_dir_path,
            source_dir_runfiles_path = paths.dirname(source_conf_file.short_path),
        ),
    ]

_sphinx_source_tree = rule(
    implementation = _sphinx_source_tree_impl,
    attrs = {
        "config": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "Config file for Sphinx",
        ),
        "deps": attr.label_list(
            providers = [SphinxDocsLibraryInfo],
        ),
        "renamed_srcs": attr.label_keyed_string_dict(
            allow_files = True,
            doc = "Doc source files for Sphinx that are renamed. This is " +
                  "typically used for files elsewhere, such as top level " +
                  "files in the repo.",
        ),
        "srcs": attr.label_list(
            allow_files = True,
            doc = "Doc source files for Sphinx.",
        ),
        "strip_prefix": attr.string(doc = "Prefix to remove from input file paths."),
    },
)
_FlagInfo = provider(
    doc = "Provider for a flag value",
    fields = ["value"],
)

def _repeated_string_list_flag_impl(ctx):
    return _FlagInfo(value = ctx.build_setting_value)

repeated_string_list_flag = rule(
    implementation = _repeated_string_list_flag_impl,
    build_setting = config.string_list(flag = True, repeatable = True),
)

def sphinx_inventory(*, name, src, **kwargs):
    """Creates a compressed inventory file from an uncompressed on.

    The Sphinx inventory format isn't formally documented, but is understood
    to be:

    ```
    # Sphinx inventory version 2
    # Project: <project name>
    # Version: <version string>
    # The remainder of this file is compressed using zlib
    name domain:role 1 relative-url display name
    ```

    Where:
      * `<project name>` is a string. e.g. `Rules Python`
      * `<version string>` is a string e.g. `1.5.3`

    And there are one or more `name domain:role ...` lines
      * `name`: the name of the symbol. It can contain special characters,
        but not spaces.
      * `domain:role`: The `domain` is usually a language, e.g. `py` or `bzl`.
        The `role` is usually the type of object, e.g. `class` or `func`. There
        is no canonical meaning to the values, they are usually domain-specific.
      * `1` is a number. It affects search priority.
      * `relative-url` is a URL path relative to the base url in the
        confg.py intersphinx config.
      * `display name` is a string. It can contain spaces, or simply be
        the value `-` to indicate it is the same as `name`

    :::{seealso}
    {bzl:obj}`//sphinxdocs/inventories` for inventories of Bazel objects.
    :::

    Args:
        name: {type}`Name` name of the target.
        src: {type}`label` Uncompressed inventory text file.
        **kwargs: {type}`dict` additional kwargs of common attributes.
    """
    _sphinx_inventory(name = name, src = src, **kwargs)

def _sphinx_inventory_impl(ctx):
    output = ctx.actions.declare_file(ctx.label.name + ".inv")
    args = ctx.actions.args()
    args.add(ctx.file.src)
    args.add(output)
    ctx.actions.run(
        executable = ctx.executable._builder,
        mnemonic = "SphinxInventoryBuilder",
        arguments = [args],
        inputs = depset([ctx.file.src]),
        outputs = [output],
    )
    return [DefaultInfo(files = depset([output]))]

_sphinx_inventory = rule(
    implementation = _sphinx_inventory_impl,
    attrs = {
        "src": attr.label(allow_single_file = True),
        "_builder": attr.label(
            default = "//sphinxdocs/private:inventory_builder",
            executable = True,
            cfg = "exec",
        ),
    },
)

def _sphinx_run_impl(ctx):
    run_info = ctx.attr.docs[_SphinxRunInfo]

    builder = ctx.attr.builder

    if builder not in run_info.per_format_args:
        builder = run_info.per_format_args.keys()[0]

    args_info = run_info.per_format_args.get(builder)
    if not args_info:
        fail("Format {} not built by {}".format(
            builder,
            ctx.attr.docs.label,
        ))

    args_str = []
    args_str.extend(args_info.args)
    args_str = "\n".join(["args+=('{}')".format(value) for value in args_info.args])
    if not args_str:
        args_str = "# empty custom args"

    env_str = "\n".join([
        "sphinx_env+=({}='{}')".format(*item)
        for item in args_info.env.items()
    ])
    if not env_str:
        env_str = "# empty custom env"

    executable = ctx.actions.declare_file(ctx.label.name)
    sphinx = run_info.sphinx
    ctx.actions.expand_template(
        template = ctx.file._template,
        output = executable,
        substitutions = {
            "%SETUP_ARGS%": args_str,
            "%SETUP_ENV%": env_str,
            "%SOURCE_DIR_EXEC_PATH%": run_info.source_tree[_SphinxSourceTreeInfo].source_root,
            "%SOURCE_DIR_RUNFILES_PATH%": run_info.source_tree[_SphinxSourceTreeInfo].source_dir_runfiles_path,
            "%SPHINX_EXEC_PATH%": sphinx[DefaultInfo].files_to_run.executable.path,
            "%SPHINX_RUNFILES_PATH%": sphinx[DefaultInfo].files_to_run.executable.short_path,
        },
        is_executable = True,
    )
    runfiles = ctx.runfiles(
        transitive_files = run_info.source_tree[DefaultInfo].files,
    ).merge(sphinx[DefaultInfo].default_runfiles).merge_all([
        tool[DefaultInfo].default_runfiles
        for tool in run_info.tools
    ])
    return [
        DefaultInfo(
            executable = executable,
            runfiles = runfiles,
        ),
    ]

sphinx_run = rule(
    implementation = _sphinx_run_impl,
    doc = """
Directly run the underlying Sphinx command `sphinx_docs` uses.

This is primarily a debugging tool. It's useful for directly running the
Sphinx command so that debuggers can be attached or output more directly
inspected without Bazel interference.
""",
    attrs = {
        "builder": attr.string(
            doc = "The output format to make runnable.",
            default = "html",
        ),
        "docs": attr.label(
            doc = "The {obj}`sphinx_docs` target to make directly runnable.",
            providers = [_SphinxRunInfo],
        ),
        "_template": attr.label(
            allow_single_file = True,
            default = "//sphinxdocs/private:sphinx_run_template.sh",
        ),
    },
    executable = True,
)
