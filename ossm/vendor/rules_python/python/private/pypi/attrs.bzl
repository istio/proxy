# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"common attributes for whl_library and pip_repository"

ATTRS = {
    "download_only": attr.bool(
        doc = """
Whether to use "pip download" instead of "pip wheel". Disables building wheels from source, but allows use of
--platform, --python-version, --implementation, and --abi in --extra_pip_args to download wheels for a different
platform from the host platform.
        """,
    ),
    "enable_implicit_namespace_pkgs": attr.bool(
        default = False,
        doc = """
If true, disables conversion of native namespace packages into pkg-util style namespace packages. When set all py_binary
and py_test targets must specify either `legacy_create_init=False` or the global Bazel option
`--incompatible_default_to_explicit_init_py` to prevent `__init__.py` being automatically generated in every directory.

This option is required to support some packages which cannot handle the conversion to pkg-util style.
            """,
    ),
    "environment": attr.string_dict(
        doc = """
Environment variables to set in the pip subprocess.
Can be used to set common variables such as `http_proxy`, `https_proxy` and `no_proxy`
Note that pip is run with "--isolated" on the CLI so `PIP_<VAR>_<NAME>`
style env vars are ignored, but env vars that control requests and urllib3
can be passed. If you need `PIP_<VAR>_<NAME>`, take a look at `extra_pip_args`
and `envsubst`.
        """,
        default = {},
    ),
    "envsubst": attr.string_list(
        mandatory = False,
        doc = """\
A list of environment variables to substitute (e.g. `["PIP_INDEX_URL",
"PIP_RETRIES"]`). The corresponding variables are expanded in `extra_pip_args`
using the syntax `$VARNAME` or `${VARNAME}` (expanding to empty string if unset)
or `${VARNAME:-default}` (expanding to default if the variable is unset or empty
in the environment). Note: On Bazel 6 and Bazel 7.0 changes to the variables named
here do not cause packages to be re-fetched. Don't fetch different things based
on the value of these variables.
""",
    ),
    "experimental_requirement_cycles": attr.string_list_dict(
        default = {},
        doc = """\
A mapping of dependency cycle names to a list of requirements which form that cycle.

Requirements which form cycles will be installed together and taken as
dependencies together in order to ensure that the cycle is always satisified.

Example:
  `sphinx` depends on `sphinxcontrib-serializinghtml`
  When listing both as requirements, ala

  ```
  py_binary(
    name = "doctool",
    ...
    deps = [
      "@pypi//sphinx:pkg",
      "@pypi//sphinxcontrib_serializinghtml",
     ]
  )
  ```

  Will produce a Bazel error such as

  ```
  ERROR: .../external/pypi_sphinxcontrib_serializinghtml/BUILD.bazel:44:6: in alias rule @pypi_sphinxcontrib_serializinghtml//:pkg: cycle in dependency graph:
      //:doctool (...)
      @pypi//sphinxcontrib_serializinghtml:pkg (...)
  .-> @pypi_sphinxcontrib_serializinghtml//:pkg (...)
  |   @pypi_sphinxcontrib_serializinghtml//:_pkg (...)
  |   @pypi_sphinx//:pkg (...)
  |   @pypi_sphinx//:_pkg (...)
  `-- @pypi_sphinxcontrib_serializinghtml//:pkg (...)
  ```

  Which we can resolve by configuring these two requirements to be installed together as a cycle

  ```
  pip_parse(
    ...
    experimental_requirement_cycles = {
      "sphinx": [
        "sphinx",
        "sphinxcontrib-serializinghtml",
      ]
    },
  )
  ```

Warning:
  If a dependency participates in multiple cycles, all of those cycles must be
  collapsed down to one. For instance `a <-> b` and `a <-> c` cannot be listed
  as two separate cycles.
""",
    ),
    "experimental_target_platforms": attr.string_list(
        default = [],
        doc = """\
A list of platforms that we will generate the conditional dependency graph for
cross platform wheels by parsing the wheel metadata. This will generate the
correct dependencies for packages like `sphinx` or `pylint`, which include
`colorama` when installed and used on Windows platforms.

An empty list means falling back to the legacy behaviour where the host
platform is the target platform.

WARNING: It may not work as expected in cases where the python interpreter
implementation that is being used at runtime is different between different platforms.
This has been tested for CPython only.

For specific target platforms use values of the form `<os>_<arch>` where `<os>`
is one of `linux`, `osx`, `windows` and arch is one of `x86_64`, `x86_32`,
`aarch64`, `s390x` and `ppc64le`.

You can also target a specific Python version by using `cp3<minor_version>_<os>_<arch>`.
If multiple python versions are specified as target platforms, then select statements
of the `lib` and `whl` targets will include usage of version aware toolchain config
settings like `@rules_python//python/config_settings:is_python_3.y`.

Special values: `host` (for generating deps for the host platform only) and
`<prefix>_*` values. For example, `cp39_*`, `linux_*`, `cp39_linux_*`.

NOTE: this is not for cross-compiling Python wheels but rather for parsing the `whl` METADATA correctly.
""",
    ),
    "extra_hub_aliases": attr.string_list_dict(
        doc = """\
Extra aliases to make for specific wheels in the hub repo. This is useful when
paired with the {attr}`whl_modifications`.

:::{versionadded} 0.38.0
:::
""",
        mandatory = False,
    ),
    "extra_pip_args": attr.string_list(
        doc = """Extra arguments to pass on to pip. Must not contain spaces.

Supports environment variables using the syntax `$VARNAME` or
`${VARNAME}` (expanding to empty string if unset) or
`${VARNAME:-default}` (expanding to default if the variable is unset
or empty in the environment), if `"VARNAME"` is listed in the
`envsubst` attribute. See also `envsubst`.
""",
    ),
    "isolated": attr.bool(
        doc = """\
Whether or not to pass the [--isolated](https://pip.pypa.io/en/stable/cli/pip/#cmdoption-isolated) flag to
the underlying pip command. Alternatively, the {envvar}`RULES_PYTHON_PIP_ISOLATED` environment variable can be used
to control this flag.
""",
        default = True,
    ),
    "pip_data_exclude": attr.string_list(
        doc = "Additional data exclusion parameters to add to the pip packages BUILD file.",
    ),
    "python_interpreter": attr.string(
        doc = """\
The python interpreter to use. This can either be an absolute path or the name
of a binary found on the host's `PATH` environment variable. If no value is set
`python3` is defaulted for Unix systems and `python.exe` for Windows.
""",
        # NOTE: This attribute should not have a default. See `_get_python_interpreter_attr`
        # default = "python3"
    ),
    "python_interpreter_target": attr.label(
        allow_single_file = True,
        doc = """
If you are using a custom python interpreter built by another repository rule,
use this attribute to specify its BUILD target. This allows pip_repository to invoke
pip using the same interpreter as your toolchain. If set, takes precedence over
python_interpreter. An example value: "@python3_x86_64-unknown-linux-gnu//:python".
""",
    ),
    "quiet": attr.bool(
        default = True,
        doc = """\
If True, suppress printing stdout and stderr output to the terminal.

If you would like to get more diagnostic output, set
{envvar}`RULES_PYTHON_REPO_DEBUG=1 <RULES_PYTHON_REPO_DEBUG>`
or
{envvar}`RULES_PYTHON_REPO_DEBUG_VERBOSITY=<INFO|DEBUG|TRACE> <RULES_PYTHON_REPO_DEBUG_VERBOSITY>`
""",
    ),
    # 600 is documented as default here: https://docs.bazel.build/versions/master/skylark/lib/repository_ctx.html#execute
    "timeout": attr.int(
        default = 600,
        doc = "Timeout (in seconds) on the rule's execution duration.",
    ),
}

def use_isolated(ctx, attr):
    """Determine whether or not to pass the pip `--isolated` flag to the pip invocation.

    Args:
        ctx: repository or module context
        attr: attributes for the repo rule or tag extension

    Returns:
        True if --isolated should be passed
    """
    use_isolated = attr.isolated

    # The environment variable will take precedence over the attribute
    isolated_env = ctx.os.environ.get("RULES_PYTHON_PIP_ISOLATED", None)
    if isolated_env != None:
        if isolated_env.lower() in ("0", "false"):
            use_isolated = False
        else:
            use_isolated = True

    return use_isolated
