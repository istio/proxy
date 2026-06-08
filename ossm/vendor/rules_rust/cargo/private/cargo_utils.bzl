"""Utility functions for the cargo rules"""

load("//rust/platform:triple_mappings.bzl", "system_to_binary_ext")

# TODO: remove after dropping support for Bazel < 7 when `abs` is a global
def abs(value):
    """Returns the absolute value of a number.

    Args:
      value (int): A number.

    Returns:
      int: The absolute value of the number.
    """
    if value < 0:
        return -value
    return value

def _resolve_repository_template(
        *,
        template,
        abi = None,
        arch = None,
        channel = None,
        system = None,
        tool = None,
        triple = None,
        vendor = None,
        version = None,
        compress_windows_names = False):
    """Render values into a repository template string

    Args:
        template (str): The template to use for rendering
        abi (str, optional): The host ABI
        arch (str, optional): The host CPU architecture
        channel (str, optional): The toolchain channel. Eg. `stable`, `nightly`.
        system (str, optional): The host system name
        tool (str, optional): The tool to expect in the particular repository.
            Eg. `cargo`, `rustc`, `stdlib`.
        triple (str, optional): The host triple
        vendor (str, optional): The host vendor name
        version (str, optional): The Rust version used in the toolchain.
        compress_windows_names (bool): Whether or not the windows repositories are
            to be in the compressed form.
    Returns:
        string: The resolved template string based on the given parameters
    """
    if abi:
        template = template.replace("{abi}", abi)

    if arch:
        template = template.replace("{arch}", arch)

    if system:
        template = template.replace("{system}", system)

    if tool:
        template = template.replace("{tool}", tool)

    if triple:
        template = template.replace("{triple}", triple)

    if vendor:
        template = template.replace("{vendor}", vendor)

    if version:
        template = template.replace("{version}", version)

    if channel:
        template = template.replace("{channel}", channel)

    if compress_windows_names and system == "windows":
        repo, _, target = template.partition("//")
        prefix = ""
        if repo.startswith("@"):
            repo = repo[1:]
            prefix = "@"
        elif repo.startswith("@@"):
            repo = repo[2:]
            prefix = "@@"

        suffix = ""
        if repo.endswith("_tools"):
            repo = repo[:-len("_tools")]
            suffix = "_tools"

        return "{}rw-{}{}//{}".format(prefix, abs(hash(repo)), suffix, target)

    return template

def get_rust_tools(
        *,
        cargo_template,
        rustc_template,
        host_triple,
        channel,
        version,
        compress_windows_names):
    """Retrieve `cargo` and `rustc` labels based on the host triple.

    Args:
        cargo_template (str): A template used to identify the label of the host `cargo` binary.
        rustc_template (str): A template used to identify the label of the host `rustc` binary.
        host_triple (struct): The host's triple. See `@rules_rust//rust/platform:triple.bzl`.
        channel (str): The Rust toolchain channel.
        version (str): The version (or iso date in case of beta or nightly channels) of Cargo+Rustc to use.
        compress_windows_names (bool): Whether or not the windows repositories are
            to be in the compressed form.

    Returns:
        struct: A struct containing the labels of expected tools
    """
    extension = system_to_binary_ext(host_triple.system)

    cargo_label = Label(_resolve_repository_template(
        template = cargo_template,
        version = version,
        channel = channel,
        triple = host_triple.str,
        arch = host_triple.arch,
        vendor = host_triple.vendor,
        system = host_triple.system,
        abi = host_triple.abi,
        tool = "cargo" + extension,
        compress_windows_names = compress_windows_names,
    ))

    rustc_label = Label(_resolve_repository_template(
        template = rustc_template,
        version = version,
        channel = channel,
        triple = host_triple.str,
        arch = host_triple.arch,
        vendor = host_triple.vendor,
        system = host_triple.system,
        abi = host_triple.abi,
        tool = "rustc" + extension,
        compress_windows_names = compress_windows_names,
    ))

    return struct(
        cargo = cargo_label,
        rustc = rustc_label,
    )
