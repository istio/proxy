"A generic rule to run a tool that appears in node_modules/.bin"

load("@rules_nodejs//nodejs:providers.bzl", "DeclarationInfo", "JSModuleInfo", "STAMP_ATTR")
load("//:providers.bzl", "ExternalNpmPackageInfo", "node_modules_aspect", "run_node")
load("//internal/common:expand_variables.bzl", "expand_variables")
load("//internal/linker:link_node_modules.bzl", "module_mappings_aspect")

# Note: this API is chosen to match nodejs_binary
# so that we can generate macros that act as either an output-producing tool or an executable
_ATTRS = {
    "args": attr.string_list(mandatory = True),
    "chdir": attr.string(),
    "configuration_env_vars": attr.string_list(default = []),
    "data": attr.label_list(allow_files = True, aspects = [module_mappings_aspect, node_modules_aspect]),
    "env": attr.string_dict(default = {}),
    "exit_code_out": attr.output(),
    "link_workspace_root": attr.bool(),
    "output_dir": attr.bool(),
    "outs": attr.output_list(),
    "silent_on_success": attr.bool(),
    "stderr": attr.output(),
    "stdout": attr.output(),
    "tool": attr.label(
        executable = True,
        cfg = "exec",
        mandatory = True,
    ),
    "stamp": STAMP_ATTR,
}

def _expand_locations(ctx, s):
    # `.split(" ")` is a work-around https://github.com/bazelbuild/bazel/issues/10309
    # _expand_locations returns an array of args to support $(execpaths) expansions.
    # TODO: If the string has intentional spaces or if one or more of the expanded file
    # locations has a space in the name, we will incorrectly split it into multiple arguments
    return ctx.expand_location(s, targets = ctx.attr.data).split(" ")

def _inputs(ctx):
    # Also include files from npm fine grained deps as inputs.
    # These deps are identified by the ExternalNpmPackageInfo provider.
    inputs_depsets = []
    for d in ctx.attr.data:
        if ExternalNpmPackageInfo in d:
            inputs_depsets.append(d[ExternalNpmPackageInfo].sources)
        if JSModuleInfo in d:
            inputs_depsets.append(d[JSModuleInfo].sources)
        if DeclarationInfo in d:
            inputs_depsets.append(d[DeclarationInfo].declarations)
    return depset(ctx.files.data, transitive = inputs_depsets).to_list()

def _impl(ctx):
    if ctx.attr.output_dir and ctx.outputs.outs:
        fail("Only one of output_dir and outs may be specified")
    if not ctx.attr.output_dir and not len(ctx.outputs.outs) and not ctx.attr.stdout and not ctx.attr.stderr:
        fail("One of output_dir, outs, stdout or stderr must be specified")

    args = ctx.actions.args()
    inputs = _inputs(ctx)
    outputs = []

    if ctx.attr.output_dir:
        outputs = [ctx.actions.declare_directory(ctx.attr.name)]
    else:
        outputs = ctx.outputs.outs

    for a in ctx.attr.args:
        args.add_all([expand_variables(ctx, e, outs = ctx.outputs.outs, output_dir = ctx.attr.output_dir) for e in _expand_locations(ctx, a)])

    envs = {}
    for k, v in ctx.attr.env.items():
        envs[k] = " ".join([expand_variables(ctx, e, outs = ctx.outputs.outs, output_dir = ctx.attr.output_dir, attribute_name = "env") for e in _expand_locations(ctx, v)])

    tool_outputs = []
    if ctx.outputs.stdout:
        tool_outputs.append(ctx.outputs.stdout)

    if ctx.outputs.stderr:
        tool_outputs.append(ctx.outputs.stderr)

    if ctx.outputs.exit_code_out:
        tool_outputs.append(ctx.outputs.exit_code_out)

    run_node(
        ctx,
        executable = "tool",
        inputs = inputs,
        outputs = outputs,
        arguments = [args],
        configuration_env_vars = ctx.attr.configuration_env_vars,
        chdir = expand_variables(ctx, ctx.attr.chdir),
        env = envs,
        stdout = ctx.outputs.stdout,
        stderr = ctx.outputs.stderr,
        exit_code_out = ctx.outputs.exit_code_out,
        silent_on_success = ctx.attr.silent_on_success,
        link_workspace_root = ctx.attr.link_workspace_root,
        mnemonic = "NpmPackageBin",
    )
    files = outputs + tool_outputs
    return [DefaultInfo(
        files = depset(files),
        runfiles = ctx.runfiles(files = files),
    )]

_npm_package_bin = rule(
    _impl,
    attrs = _ATTRS,
)

def npm_package_bin(
        tool = None,
        package = None,
        package_bin = None,
        data = [],
        env = {},
        outs = [],
        args = [],
        stderr = None,
        stdout = None,
        exit_code_out = None,
        output_dir = False,
        link_workspace_root = False,
        chdir = None,
        silent_on_success = False,
        **kwargs):
    """Run an arbitrary npm package binary (e.g. a program under node_modules/.bin/*) under Bazel.

    It must produce outputs. If you just want to run a program with `bazel run`, use the nodejs_binary rule.

    This is like a genrule() except that it runs our launcher script that first
    links the node_modules tree before running the program.

    By default, Bazel runs actions with a working directory set to your workspace root.
    Use the `chdir` attribute to change the working directory before the program runs.

    This is a great candidate to wrap with a macro, as documented:
    https://docs.bazel.build/versions/main/skylark/macros.html#full-example

    Args:
        data: similar to [genrule.srcs](https://docs.bazel.build/versions/main/be/general.html#genrule.srcs)
              may also include targets that produce or reference npm packages which are needed by the tool
        outs: similar to [genrule.outs](https://docs.bazel.build/versions/main/be/general.html#genrule.outs)
        output_dir: set to True if you want the output to be a directory
                 Exactly one of `outs`, `output_dir` may be used.
                 If you output a directory, there can only be one output, which will be a directory named the same as the target.
        stderr: set to capture the stderr of the binary to a file, which can later be used as an input to another target
                subject to the same semantics as `outs`
        stdout: set to capture the stdout of the binary to a file, which can later be used as an input to another target
                subject to the same semantics as `outs`
        exit_code_out: set to capture the exit code of the binary to a file, which can later be used as an input to another target
                subject to the same semantics as `outs`. Note that setting this will force the binary to exit 0.
                If the binary creates outputs and these are declared, they must still be created
        silent_on_success: produce no output on stdout nor stderr when program exits with status code 0.
                This makes node binaries match the expected bazel paradigm.

        args: Command-line arguments to the tool.

            Subject to 'Make variable' substitution. See https://docs.bazel.build/versions/main/be/make-variables.html.

            1. Predefined source/output path substitions is applied first:

            See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.

            Use $(execpath) $(execpaths) to expand labels to the execroot (where Bazel runs build actions).

            Use $(rootpath) $(rootpaths) to expand labels to the runfiles path that a built binary can use
            to find its dependencies.

            Since npm_package_bin is used primarily for build actions, in most cases you'll want to
            use $(execpath) or $(execpaths) to expand locations.

            Using $(location) and $(locations) expansions is not recommended as these are a synonyms
            for either $(execpath) or $(rootpath) depending on the context.

            2. "Make" variables are expanded second:

            Predefined "Make" variables such as $(COMPILATION_MODE) and $(TARGET_CPU) are expanded.
            See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_variables.

            Like genrule, you may also use some syntax sugar for locations.

            - `$@`: if you have only one output file, the location of the output
            - `$(@D)`: The output directory. If output_dir=False and there is only one file name in outs, this expands to the directory
                containing that file. If there are multiple files, this instead expands to the package's root directory in the genfiles
                tree, even if all generated files belong to the same subdirectory! If output_dir=True then this corresponds
                to the output directory which is the $(RULEDIR)/{target_name}.
            - `$(RULEDIR)`: the root output directory of the rule, corresponding with its package
                (can be used with output_dir=True or False)

            See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_genrule_variables.

            Custom variables are also expanded including variables set through the Bazel CLI with --define=SOME_VAR=SOME_VALUE.
            See https://docs.bazel.build/versions/main/be/make-variables.html#custom_variables.

        package: an npm package whose binary to run, like "terser". Assumes your node_modules are installed in a workspace called "npm"
        package_bin: the "bin" entry from `package` that should be run. By default package_bin is the same string as `package`
        tool: a label for a binary to run, like `@npm//terser/bin:terser`. This is the longer form of package/package_bin.
              Note that you can also refer to a binary in your local workspace.
        link_workspace_root: Link the workspace root to the bin_dir to support absolute requires like 'my_wksp/path/to/file'.
              If source files need to be required then they can be copied to the bin_dir with copy_to_bin.
        chdir: Working directory to run the binary or test in, relative to the workspace.

            By default, Bazel always runs in the workspace root.

            To run in the directory containing the `npm_package_bin` under the source tree, use
            `chdir = package_name()`
            (or if you're in a macro, use `native.package_name()`).

            To run in the output directory where the npm_package_bin writes outputs, use
            `chdir = "$(RULEDIR)"`

            WARNING: this will affect other paths passed to the program, either as arguments or in configuration files,
            which are workspace-relative.
            You may need `../../` segments to re-relativize such paths to the new working directory.
            In a `BUILD` file you could do something like this to point to the output path:

            ```python
            _package_segments = len(package_name().split("/"))
            npm_package_bin(
                ...
                chdir = package_name(),
                # ../.. segments to re-relative paths from the chdir back to workspace
                args = ["/".join([".."] * _package_segments + ["$@"])],
            )
            ```
        env: specifies additional environment variables to set when the target is executed. The values of environment variables
            are subject to 'Make variable' substitution (see [args](#npm_package_bin-args)).
        **kwargs: additional undocumented keyword args
    """
    if not tool:
        if not package:
            fail("You must supply either the tool or package attribute")
        if not package_bin:
            package_bin = package
        tool = "@npm//%s/bin:%s" % (package, package_bin)
    _npm_package_bin(
        data = data,
        outs = outs,
        args = args,
        chdir = chdir,
        env = env,
        stdout = stdout,
        stderr = stderr,
        exit_code_out = exit_code_out,
        output_dir = output_dir,
        tool = tool,
        link_workspace_root = link_workspace_root,
        silent_on_success = silent_on_success,
        **kwargs
    )
