# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""An implementation for a simple macro to lock the requirements.
"""

load("@bazel_skylib//lib:shell.bzl", "shell")
load("//python:py_binary.bzl", "py_binary")
load("//python/private:bzlmod_enabled.bzl", "BZLMOD_ENABLED")  # buildifier: disable=bzl-visibility
load("//python/private:toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility
load(":toolchain_types.bzl", "UV_TOOLCHAIN_TYPE")

visibility(["//..."])

_PYTHON_VERSION_FLAG = "//python/config_settings:python_version"

_RunLockInfo = provider(
    doc = "",
    fields = {
        "args": "The args passed to the `uv` by default when running the runnable target.",
        "env": "The env passed to the execution.",
        "srcs": "Source files required to run the runnable target.",
    },
)

def _args(ctx):
    """A small helper to ensure that the right args are pushed to the _RunLockInfo provider"""
    run_info = []
    args = ctx.actions.args()

    def _add_args(arg, maybe_value = None):
        run_info.append(arg)
        if maybe_value:
            args.add(arg, maybe_value)
            run_info.append(maybe_value)
        else:
            args.add(arg)

    def _add_all(name, all_args = None, **kwargs):
        if not all_args and type(name) == "list":
            all_args = name
            name = None

        before_each = kwargs.get("before_each")
        if name:
            args.add_all(name, all_args, **kwargs)
            run_info.append(name)
        else:
            args.add_all(all_args, **kwargs)

        for arg in all_args:
            if before_each:
                run_info.append(before_each)
            run_info.append(arg)

    return struct(
        run_info = run_info,
        run_shell = args,
        add = _add_args,
        add_all = _add_all,
    )

def _lock_impl(ctx):
    srcs = ctx.files.srcs
    fname = "{}.out".format(ctx.label.name)
    python_version = ctx.attr.python_version
    if python_version:
        fname = "{}.{}.out".format(
            ctx.label.name,
            python_version.replace(".", "_"),
        )

    output = ctx.actions.declare_file(fname)
    toolchain_info = ctx.toolchains[UV_TOOLCHAIN_TYPE]
    uv = toolchain_info.uv_toolchain_info.uv[DefaultInfo].files_to_run.executable

    args = _args(ctx)
    args.add_all([
        uv,
        "pip",
        "compile",
        "--no-python-downloads",
        "--no-cache",
    ])
    pkg = ctx.label.package
    update_target = ctx.attr.update_target
    args.add("--custom-compile-command", "bazel run //{}:{}".format(pkg, update_target))
    if ctx.attr.generate_hashes:
        args.add("--generate-hashes")
    if not ctx.attr.strip_extras:
        args.add("--no-strip-extras")
    args.add_all(ctx.files.build_constraints, before_each = "--build-constraints")
    args.add_all(ctx.files.constraints, before_each = "--constraints")
    args.add_all(ctx.attr.args)

    exec_tools = ctx.toolchains[EXEC_TOOLS_TOOLCHAIN_TYPE].exec_tools
    runtime = exec_tools.exec_interpreter[platform_common.ToolchainInfo].py3_runtime
    python = runtime.interpreter or runtime.interpreter_path
    python_files = runtime.files
    args.add("--python", python)
    args.add_all(srcs)

    args.run_shell.add("--output-file", output)

    # These arguments does not change behaviour, but it reduces the output from
    # the command, which is especially verbose in stderr.
    args.run_shell.add("--no-progress")
    args.run_shell.add("--quiet")

    if ctx.files.existing_output:
        command = '{python} -c {python_cmd} && "$@"'.format(
            python = getattr(python, "path", python),
            python_cmd = shell.quote(
                "from shutil import copy; copy(\"{src}\", \"{dst}\")".format(
                    src = ctx.files.existing_output[0].path,
                    dst = output.path,
                ),
            ),
        )
    else:
        command = '"$@"'

    srcs = srcs + ctx.files.build_constraints + ctx.files.constraints

    ctx.actions.run_shell(
        command = command,
        inputs = srcs + ctx.files.existing_output,
        mnemonic = "PyRequirementsLockUv",
        outputs = [output],
        arguments = [args.run_shell],
        tools = [
            uv,
            python_files,
        ],
        progress_message = "Creating a requirements.txt with uv: %{label}",
        env = ctx.attr.env,
    )

    return [
        DefaultInfo(files = depset([output])),
        _RunLockInfo(
            args = args.run_info,
            env = ctx.attr.env,
            srcs = depset(
                srcs + [uv],
                transitive = [python_files],
            ),
        ),
    ]

def _transition_impl(input_settings, attr):
    settings = {
        _PYTHON_VERSION_FLAG: input_settings[_PYTHON_VERSION_FLAG],
    }
    if attr.python_version:
        settings[_PYTHON_VERSION_FLAG] = attr.python_version
    return settings

_python_version_transition = transition(
    implementation = _transition_impl,
    inputs = [_PYTHON_VERSION_FLAG],
    outputs = [_PYTHON_VERSION_FLAG],
)

_lock = rule(
    implementation = _lock_impl,
    doc = """\
The lock rule that does the locking in a build action (that makes it possible
to use RBE) and also prepares information for a `bazel run` executable rule.
""",
    attrs = {
        "args": attr.string_list(
            doc = "Public, see the docs in the macro.",
        ),
        "build_constraints": attr.label_list(
            allow_files = True,
            doc = "Public, see the docs in the macro.",
        ),
        "constraints": attr.label_list(
            allow_files = True,
            doc = "Public, see the docs in the macro.",
        ),
        "env": attr.string_dict(
            doc = "Public, see the docs in the macro.",
        ),
        "existing_output": attr.label(
            mandatory = False,
            allow_single_file = True,
            doc = """\
An already existing output file that is used as a basis for further
modifications and the locking is not done from scratch.
""",
        ),
        "generate_hashes": attr.bool(
            doc = "Public, see the docs in the macro.",
            default = True,
        ),
        "output": attr.string(
            doc = "Public, see the docs in the macro.",
            mandatory = True,
        ),
        "python_version": attr.string(
            doc = "Public, see the docs in the macro.",
        ),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
            doc = "Public, see the docs in the macro.",
        ),
        "strip_extras": attr.bool(
            doc = "Public, see the docs in the macro.",
            default = False,
        ),
        "update_target": attr.string(
            mandatory = True,
            doc = """\
The string to input for the 'uv pip compile'.
""",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    toolchains = [
        EXEC_TOOLS_TOOLCHAIN_TYPE,
        UV_TOOLCHAIN_TYPE,
    ],
    cfg = _python_version_transition,
)

def _lock_run_impl(ctx):
    if ctx.attr.is_windows:
        path_sep = "\\"
        ext = ".exe"
    else:
        path_sep = "/"
        ext = ""

    def _maybe_path(arg):
        if hasattr(arg, "short_path"):
            arg = arg.short_path

        return shell.quote(arg.replace("/", path_sep))

    info = ctx.attr.lock[_RunLockInfo]
    executable = ctx.actions.declare_file(ctx.label.name + ext)
    ctx.actions.expand_template(
        template = ctx.files._template[0],
        substitutions = {
            '"{{args}}"': " ".join([_maybe_path(arg) for arg in info.args]),
            "{{src_out}}": "{}/{}".format(ctx.label.package, ctx.attr.output).replace(
                "/",
                path_sep,
            ),
        },
        output = executable,
        is_executable = True,
    )

    return [
        DefaultInfo(
            executable = executable,
            runfiles = ctx.runfiles(transitive_files = info.srcs),
        ),
        RunEnvironmentInfo(
            environment = info.env,
        ),
    ]

_lock_run = rule(
    implementation = _lock_run_impl,
    doc = """\
""",
    attrs = {
        "is_windows": attr.bool(mandatory = True),
        "lock": attr.label(
            doc = "The lock target that is doing locking in a build action.",
            providers = [_RunLockInfo],
            cfg = "exec",
        ),
        "output": attr.string(
            doc = """\
The output that we would be updated, relative to the package the macro is used in.
""",
        ),
        "_template": attr.label(
            default = "//python/uv/private:lock_template",
            doc = """\
The template to be used for 'uv pip compile'. This is either .ps1 or bash
script depending on what the target platform is executed on.
""",
        ),
    },
    executable = True,
)

def _maybe_file(path):
    """A small function to return a list of existing outputs.

    If the file referenced by the input argument exists, then it will return
    it, otherwise it will return an empty list. This is useful to for programs
    like pip-compile which behave differently if the output file exists and
    update the output file in place.

    The API of the function ensures that path is not a glob itself.

    Args:
        path: {type}`str` the file name.
    """
    for p in native.glob([path], allow_empty = True):
        if path == p:
            return p

    return None

def _expand_template_impl(ctx):
    pkg = ctx.label.package
    update_src = ctx.actions.declare_file(ctx.attr.update_target + ".py")

    # Fix the path construction to avoid absolute paths
    # If package is empty (root), don't add a leading slash
    dst = "{}/{}".format(pkg, ctx.attr.output) if pkg else ctx.attr.output

    ctx.actions.expand_template(
        template = ctx.files._template[0],
        substitutions = {
            "{{dst}}": dst,
            "{{src}}": "{}".format(ctx.files.src[0].short_path),
            "{{update_target}}": "//{}:{}".format(pkg, ctx.attr.update_target),
        },
        output = update_src,
    )
    return DefaultInfo(files = depset([update_src]))

_expand_template = rule(
    implementation = _expand_template_impl,
    attrs = {
        "output": attr.string(mandatory = True),
        "src": attr.label(mandatory = True),
        "update_target": attr.string(mandatory = True),
        "_template": attr.label(
            default = "//python/uv/private:lock_copier.py",
            allow_single_file = True,
        ),
    },
    doc = "Expand the template for the update script allowing us to use `select` statements in the {attr}`output` attribute.",
)

def lock(
        *,
        name,
        srcs,
        out,
        args = [],
        build_constraints = [],
        constraints = [],
        env = None,
        generate_hashes = True,
        python_version = None,
        strip_extras = False,
        **kwargs):
    """Pin the requirements based on the src files.

    This macro creates the following targets:
    - `name`: the target that creates the requirements.txt file in a build
      action. This target will have `no-cache` and `requires-network` added
      to its tags.
    - `name.run`: a runnable target that can be used to pass extra parameters
      to the same command that would be run in the `name` action. This will
      update the source copy of the requirements file. You can customize the
      args via the command line, but it requires being able to run `uv` (and
      possibly `python`) directly on your host.
    - `name.update`: a target that can be run to update the source-tree version
      of the requirements lock file. The output can be fed to the
      {obj}`pip.parse` bzlmod extension tag class. Note, you can use
      `native_test` to wrap this target to make a test. You can't customize the
      args via command line, but you can use RBE to generate requirements
      (offload execution and run for different platforms). Note, that for RBE
      to be usable, one needs to ensure that the nodes running the action have
      internet connectivity or the indexes are provided in a different way for
      a fully offline operation.

    :::{note}
    All of the targets have `manual` tags as locking results cannot be cached.
    :::

    Args:
        name: {type}`str` The prefix of all targets created by this macro.
        srcs: {type}`list[Label]` The sources that will be used. Add all of the
            files that would be passed as srcs to the `uv pip compile` command.
        out: {type}`str` The output file relative to the package.
        args: {type}`list[str]` The list of args to pass to uv. Note, these are
            written into the runnable `name.run` target.
        env: {type}`dict[str, str]` the environment variables to set. Note, this
            is passed as is and the environment variables are not expanded.
        build_constraints: {type}`list[Label]` The list of build constraints to use.
        constraints: {type}`list[Label]` The list of constraints files to use.
        generate_hashes: {type}`bool` Generate hashes for all of the
            requirements. This is a must if you want to use
            {attr}`pip.parse.experimental_index_url`. Defaults to `True`.
        strip_extras: {type}`bool` whether to strip extras from the output.
            Currently `rules_python` requires `--no-strip-extras` to properly
            function, but sometimes one may want to not have the extras if you
            are compiling the requirements file for using it as a constraints
            file. Defaults to `False`.
        python_version: {type}`str | None` the python_version to transition to
            when locking the requirements. Defaults to the default python version
            configured by the {obj}`python` module extension.
        **kwargs: common kwargs passed to rules.
    """
    update_target = "{}.update".format(name)
    locker_target = "{}.run".format(name)

    # Check if the output file already exists, if yes, first copy it to the
    # output file location in order to make `uv` not change the requirements if
    # we are just running the command.
    maybe_out = _maybe_file(out)

    tags = ["manual"] + kwargs.pop("tags", [])
    if not BZLMOD_ENABLED:
        kwargs["target_compatible_with"] = ["@platforms//:incompatible"]

    _lock(
        name = name,
        args = args,
        build_constraints = build_constraints,
        constraints = constraints,
        env = env,
        existing_output = maybe_out,
        generate_hashes = generate_hashes,
        python_version = python_version,
        srcs = srcs,
        strip_extras = strip_extras,
        update_target = update_target,
        output = out,
        tags = [
            "no-cache",
            "requires-network",
        ] + tags,
        **kwargs
    )

    # A target for updating the in-tree version directly by skipping the in-action
    # uv pip compile.
    _lock_run(
        name = locker_target,
        lock = name,
        output = out,
        is_windows = select({
            "@platforms//os:windows": True,
            "//conditions:default": False,
        }),
        tags = tags,
        **kwargs
    )

    # FIXME @aignas 2025-03-20: is it possible to extend `py_binary` so that the
    # srcs are generated before `py_binary` is run? I found that
    # `ctx.files.srcs` usage in the base implementation is making it difficult.
    template_target = "_{}_gen".format(name)
    _expand_template(
        name = template_target,
        src = name,
        output = out,
        update_target = update_target,
        tags = tags,
    )

    py_binary(
        name = update_target,
        srcs = [template_target],
        data = [name] + ([maybe_out] if maybe_out else []),
        tags = tags,
        **kwargs
    )
