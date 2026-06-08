"""Common utilities useful for unifying the behavior of different parts of `cargo-bazel`."""

# buildifier: disable=bzl-visibility
load("//cargo/private:cargo_utils.bzl", _rust_get_rust_tools = "get_rust_tools")
load("//rust/platform:triple.bzl", _get_host_triple = "get_host_triple")

get_host_triple = _get_host_triple

CARGO_BAZEL_ISOLATED = "CARGO_BAZEL_ISOLATED"
CARGO_BAZEL_REPIN = "CARGO_BAZEL_REPIN"
CARGO_BAZEL_DEBUG = "CARGO_BAZEL_DEBUG"
CARGO_BAZEL_TIMEOUT = "CARGO_BAZEL_TIMEOUT"
REPIN = "REPIN"

CARGO_BAZEL_REPIN_ONLY = "CARGO_BAZEL_REPIN_ONLY"

REPIN_ENV_VARS = [
    CARGO_BAZEL_REPIN,
    REPIN,
]

REPIN_ALLOWLIST_ENV_VAR = CARGO_BAZEL_REPIN_ONLY

_EXECUTE_ERROR_MESSAGE = """\
Command {args} failed with exit code {exit_code}.
STDOUT ------------------------------------------------------------------------
{stdout}
STDERR ------------------------------------------------------------------------
{stderr}
"""

def execute(repository_ctx, args, env = {}, allow_fail = False, quiet = True):
    """A helper macro for executing some arguments and displaying nicely formatted errors

    Args:
        repository_ctx (repository_ctx): The rule's context object.
        args (list): A list of strings which act as `argv` for execution.
        env (dict, optional): Environment variables to set in the execution environment.
        allow_fail (bool, optional): Allow the process to fail.
        quiet (bool): Whether or not to print output from the executable.

    Returns:
        struct: The results of `repository_ctx.execute`
    """

    if repository_ctx.os.environ.get(CARGO_BAZEL_DEBUG, None):
        quiet = False

    exec_kwargs = dict(environment = env, quiet = quiet)

    timeout = repository_ctx.os.environ.get(CARGO_BAZEL_TIMEOUT, None)
    if timeout:
        exec_kwargs.update(timeout = int(timeout))

    result = repository_ctx.execute(
        args,
        **exec_kwargs
    )

    if result.return_code and not allow_fail:
        fail(_EXECUTE_ERROR_MESSAGE.format(
            args = args,
            exit_code = result.return_code,
            stdout = result.stdout,
            stderr = result.stderr,
        ))

    return result

def new_cargo_bazel_fn(
        repository_ctx,
        cargo_bazel_path,
        cargo_path,
        rustc_path,
        isolated = True,
        quiet = False):
    """A helper function to allow executing cargo_bazel in repository rules module extensions.

    Args:
        repository_ctx (repository_ctx): The repository rule or module extension's context.
        cargo_bazel_path (path): Path The path to a `cargo-bazel` binary
        cargo_path (path): Path to a Cargo binary.
        rustc_path (path): Path to a rustc binary.
        isolated (bool): Enable isolation upon request.
        quiet (bool): Whether or not to print output from the executable.
    Returns:
        A function that can be called to execute cargo_bazel.
    """

    # Placing this as a nested function allows users to call this right at the
    # start of a module extension, thus triggering any restarts as early as
    # possible (since module_ctx.path triggers restarts).
    def _execute(args, env = {}, allow_fail = False):
        return execute(
            repository_ctx,
            args = [
                cargo_bazel_path,
            ] + args + [
                "--cargo",
                cargo_path,
                "--rustc",
                rustc_path,
            ],
            env = {
                "CARGO": str(cargo_path),
                "RUSTC": str(rustc_path),
            } | cargo_environ(repository_ctx, isolated = isolated) | env,
            allow_fail = allow_fail,
            quiet = quiet,
        )

    return _execute

def get_rust_tools(repository_ctx, host_triple):
    """Retrieve a cargo and rustc binary based on the host triple.

    Args:
        repository_ctx (repository_ctx): The rule's context object.
        host_triple (struct): A `@rules_rust//rust:triple.bzl%triple` object.

    Returns:
        struct: A struct containing the expected rust tools
    """

    # This is a bit hidden but to ensure Cargo behaves consistently based
    # on the user provided config file, the config file is installed into
    # the `CARGO_HOME` path. This is done so here since fetching tools
    # is expected to always occur before any subcommands are run.
    if repository_ctx.attr.isolated and repository_ctx.attr.cargo_config:
        cargo_home = _cargo_home_path(repository_ctx)
        cargo_home_config = repository_ctx.path("{}/config.toml".format(cargo_home))
        cargo_config = repository_ctx.path(repository_ctx.attr.cargo_config)
        repository_ctx.symlink(cargo_config, cargo_home_config)

    if repository_ctx.attr.rust_version.startswith(("beta", "nightly")):
        channel, _, version = repository_ctx.attr.rust_version.partition("/")
    else:
        channel = "stable"
        version = repository_ctx.attr.rust_version

    return _rust_get_rust_tools(
        cargo_template = repository_ctx.attr.rust_toolchain_cargo_template,
        rustc_template = repository_ctx.attr.rust_toolchain_rustc_template,
        host_triple = host_triple,
        channel = channel,
        version = version,
        compress_windows_names = repository_ctx.attr.compressed_windows_toolchain_names,
    )

def _cargo_home_path(repository_ctx):
    """Define a path within the repository to use in place of `CARGO_HOME`

    Args:
        repository_ctx (repository_ctx): The rules context object

    Returns:
        path: The path to a directory to use as `CARGO_HOME`
    """
    return repository_ctx.path(".cargo_home")

def cargo_environ(repository_ctx, isolated = True):
    """Define Cargo environment variables for use with `cargo-bazel`

    Args:
        repository_ctx (repository_ctx): The rules context object
        isolated (bool): Enable isolation upon request.

    Returns:
        dict: A set of environment variables for `cargo-bazel` executions
    """
    env = dict()

    if CARGO_BAZEL_ISOLATED in repository_ctx.os.environ:
        if repository_ctx.os.environ[CARGO_BAZEL_ISOLATED].lower() in ["true", "1", "yes", "on"]:
            env.update({
                "CARGO_HOME": str(_cargo_home_path(repository_ctx)),
            })
    elif isolated:
        env.update({
            "CARGO_HOME": str(_cargo_home_path(repository_ctx)),
        })

    return env

def parse_alias_rule(value):
    """Attempts to parse an `AliasRule` from supplied string.

    Args:
        value (str): String value to be parsed.

    Returns:
        value: A Rust compatible `AliasRule`.
    """
    if value == None:
        return None

    if value == "alias" or value == "dbg" or value == "fastbuild" or value == "opt":
        return value

    if value.count(":") != 2:
        fail("Invalid custom value for `alias_rule`.\n{}\nValues must be in the format '<label to .bzl>:<rule>'.".format(value))

    split = value.rsplit(":", 1)
    bzl = Label(split[0])
    rule = split[1]

    if rule == "alias":
        fail("Custom value rule cannot be named `alias`.\n{}".format(value))

    return struct(
        bzl = str(bzl),
        rule = rule,
    )
