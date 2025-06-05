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

""

load("@bazel_skylib//lib:sets.bzl", "sets")
load("//python/private:normalize_name.bzl", "normalize_name")
load("//python/private:repo_utils.bzl", "REPO_DEBUG_ENV_VAR")
load("//python/private:text_util.bzl", "render")
load(":evaluate_markers.bzl", "evaluate_markers", EVALUATE_MARKERS_SRCS = "SRCS")
load(":parse_requirements.bzl", "host_platform", "parse_requirements", "select_requirement")
load(":pip_repository_attrs.bzl", "ATTRS")
load(":render_pkg_aliases.bzl", "render_pkg_aliases")
load(":requirements_files_by_platform.bzl", "requirements_files_by_platform")

def _get_python_interpreter_attr(rctx):
    """A helper function for getting the `python_interpreter` attribute or it's default

    Args:
        rctx (repository_ctx): Handle to the rule repository context.

    Returns:
        str: The attribute value or it's default
    """
    if rctx.attr.python_interpreter:
        return rctx.attr.python_interpreter

    if "win" in rctx.os.name:
        return "python.exe"
    else:
        return "python3"

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

_BUILD_FILE_CONTENTS = """\
package(default_visibility = ["//visibility:public"])

# Ensure the `requirements.bzl` source can be accessed by stardoc, since users load() from it
exports_files(["requirements.bzl"])
"""

def _pip_repository_impl(rctx):
    requirements_by_platform = parse_requirements(
        rctx,
        requirements_by_platform = requirements_files_by_platform(
            requirements_by_platform = rctx.attr.requirements_by_platform,
            requirements_linux = rctx.attr.requirements_linux,
            requirements_lock = rctx.attr.requirements_lock,
            requirements_osx = rctx.attr.requirements_darwin,
            requirements_windows = rctx.attr.requirements_windows,
            extra_pip_args = rctx.attr.extra_pip_args,
        ),
        extra_pip_args = rctx.attr.extra_pip_args,
        evaluate_markers = lambda rctx, requirements: evaluate_markers(
            rctx,
            requirements = requirements,
            python_interpreter = rctx.attr.python_interpreter,
            python_interpreter_target = rctx.attr.python_interpreter_target,
            srcs = rctx.attr._evaluate_markers_srcs,
        ),
    )
    selected_requirements = {}
    options = None
    repository_platform = host_platform(rctx)
    for name, requirements in requirements_by_platform.items():
        r = select_requirement(
            requirements,
            platform = None if rctx.attr.download_only else repository_platform,
        )
        if not r:
            continue
        options = options or r.extra_pip_args
        selected_requirements[name] = r.srcs.requirement_line

    bzl_packages = sorted(selected_requirements.keys())

    # Normalize cycles first
    requirement_cycles = {
        name: sorted(sets.to_list(sets.make(deps)))
        for name, deps in rctx.attr.experimental_requirement_cycles.items()
    }

    # Check for conflicts between cycles _before_ we normalize package names so
    # that reported errors use the names the user specified
    for i in range(len(requirement_cycles)):
        left_group = requirement_cycles.keys()[i]
        left_deps = requirement_cycles.values()[i]
        for j in range(len(requirement_cycles) - (i + 1)):
            right_deps = requirement_cycles.values()[1 + i + j]
            right_group = requirement_cycles.keys()[1 + i + j]
            for d in left_deps:
                if d in right_deps:
                    fail("Error: Requirement %s cannot be repeated between cycles %s and %s; please merge the cycles." % (d, left_group, right_group))

    # And normalize the names as used in the cycle specs
    #
    # NOTE: We must check that a listed dependency is actually in the actual
    # requirements set for the current platform so that we can support cycles in
    # platform-conditional requirements. Otherwise we'll blindly generate a
    # label referencing a package which may not be installed on the current
    # platform.
    requirement_cycles = {
        normalize_name(name): sorted([normalize_name(d) for d in group if normalize_name(d) in bzl_packages])
        for name, group in requirement_cycles.items()
    }

    imports = [
        # NOTE: Maintain the order consistent with `buildifier`
        'load("@rules_python//python:pip.bzl", "pip_utils")',
        'load("@rules_python//python/pip_install:pip_repository.bzl", "group_library", "whl_library")',
    ]

    annotations = {}
    for pkg, annotation in rctx.attr.annotations.items():
        filename = "{}.annotation.json".format(normalize_name(pkg))
        rctx.file(filename, json.encode_indent(json.decode(annotation)))
        annotations[pkg] = "@{name}//:{filename}".format(name = rctx.attr.name, filename = filename)

    config = {
        "download_only": rctx.attr.download_only,
        "enable_implicit_namespace_pkgs": rctx.attr.enable_implicit_namespace_pkgs,
        "environment": rctx.attr.environment,
        "envsubst": rctx.attr.envsubst,
        "extra_pip_args": options,
        "isolated": use_isolated(rctx, rctx.attr),
        "pip_data_exclude": rctx.attr.pip_data_exclude,
        "python_interpreter": _get_python_interpreter_attr(rctx),
        "quiet": rctx.attr.quiet,
        "repo": rctx.attr.name,
        "timeout": rctx.attr.timeout,
    }
    if rctx.attr.use_hub_alias_dependencies:
        config["dep_template"] = "@{}//{{name}}:{{target}}".format(rctx.attr.name)
    else:
        config["repo_prefix"] = "{}_".format(rctx.attr.name)

    if rctx.attr.python_interpreter_target:
        config["python_interpreter_target"] = str(rctx.attr.python_interpreter_target)
    if rctx.attr.experimental_target_platforms:
        config["experimental_target_platforms"] = rctx.attr.experimental_target_platforms

    macro_tmpl = "@%s//{}:{}" % rctx.attr.name

    aliases = render_pkg_aliases(
        aliases = {
            pkg: rctx.attr.name + "_" + pkg
            for pkg in bzl_packages or []
        },
        extra_hub_aliases = rctx.attr.extra_hub_aliases,
        requirement_cycles = requirement_cycles,
    )
    for path, contents in aliases.items():
        rctx.file(path, contents)

    rctx.file("BUILD.bazel", _BUILD_FILE_CONTENTS)
    rctx.template("requirements.bzl", rctx.attr._template, substitutions = {
        "    # %%GROUP_LIBRARY%%": """\
    group_repo = "{name}__groups"
    group_library(
        name = group_repo,
        repo_prefix = "{name}_",
        groups = all_requirement_groups,
    )""".format(name = rctx.attr.name) if not rctx.attr.use_hub_alias_dependencies else "",
        "%%ALL_DATA_REQUIREMENTS%%": render.list([
            macro_tmpl.format(p, "data")
            for p in bzl_packages
        ]),
        "%%ALL_REQUIREMENTS%%": render.list([
            macro_tmpl.format(p, "pkg")
            for p in bzl_packages
        ]),
        "%%ALL_REQUIREMENT_GROUPS%%": render.dict(requirement_cycles),
        "%%ALL_WHL_REQUIREMENTS_BY_PACKAGE%%": render.dict({
            p: macro_tmpl.format(p, "whl")
            for p in bzl_packages
        }),
        "%%ANNOTATIONS%%": render.dict(dict(sorted(annotations.items()))),
        "%%CONFIG%%": render.dict(dict(sorted(config.items()))),
        "%%EXTRA_PIP_ARGS%%": json.encode(options),
        "%%IMPORTS%%": "\n".join(imports),
        "%%MACRO_TMPL%%": macro_tmpl,
        "%%NAME%%": rctx.attr.name,
        "%%PACKAGES%%": render.list(
            [
                ("{}_{}".format(rctx.attr.name, p), r)
                for p, r in sorted(selected_requirements.items())
            ],
        ),
    })

    return

pip_repository = repository_rule(
    attrs = dict(
        annotations = attr.string_dict(
            doc = """\
Optional annotations to apply to packages. Keys should be package names, with
capitalization matching the input requirements file, and values should be
generated using the `package_name` macro. For example usage, see [this WORKSPACE
file](https://github.com/bazelbuild/rules_python/blob/main/examples/pip_repository_annotations/WORKSPACE).
""",
        ),
        _template = attr.label(
            default = ":requirements.bzl.tmpl.workspace",
        ),
        _evaluate_markers_srcs = attr.label_list(
            default = EVALUATE_MARKERS_SRCS,
            doc = """\
The list of labels to use as SRCS for the marker evaluation code. This ensures that the
code will be re-evaluated when any of files in the default changes.
""",
        ),
        **ATTRS
    ),
    doc = """Accepts a locked/compiled requirements file and installs the dependencies listed within.

Those dependencies become available in a generated `requirements.bzl` file.
You can instead check this `requirements.bzl` file into your repo, see the "vendoring" section below.

In your WORKSPACE file:

```starlark
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "pypi",
    requirements_lock = ":requirements.txt",
)

load("@pypi//:requirements.bzl", "install_deps")

install_deps()
```

You can then reference installed dependencies from a `BUILD` file with the alias targets generated in the same repo, for example, for `PyYAML` we would have the following:
- `@pypi//pyyaml` and `@pypi//pyyaml:pkg` both point to the `py_library`
  created after extracting the `PyYAML` package.
- `@pypi//pyyaml:data` points to the extra data included in the package.
- `@pypi//pyyaml:dist_info` points to the `dist-info` files in the package.
- `@pypi//pyyaml:whl` points to the wheel file that was extracted.

```starlark
py_library(
    name = "bar",
    ...
    deps = [
       "//my/other:dep",
       "@pypi//numpy",
       "@pypi//requests",
    ],
)
```

or

```starlark
load("@pypi//:requirements.bzl", "requirement")

py_library(
    name = "bar",
    ...
    deps = [
       "//my/other:dep",
       requirement("numpy"),
       requirement("requests"),
    ],
)
```

In addition to the `requirement` macro, which is used to access the generated `py_library`
target generated from a package's wheel, The generated `requirements.bzl` file contains
functionality for exposing [entry points][whl_ep] as `py_binary` targets as well.

[whl_ep]: https://packaging.python.org/specifications/entry-points/

```starlark
load("@pypi//:requirements.bzl", "entry_point")

alias(
    name = "pip-compile",
    actual = entry_point(
        pkg = "pip-tools",
        script = "pip-compile",
    ),
)
```

Note that for packages whose name and script are the same, only the name of the package
is needed when calling the `entry_point` macro.

```starlark
load("@pip//:requirements.bzl", "entry_point")

alias(
    name = "flake8",
    actual = entry_point("flake8"),
)
```

:::{rubric} Vendoring the requirements.bzl file
:heading-level: 3
:::

In some cases you may not want to generate the requirements.bzl file as a repository rule
while Bazel is fetching dependencies. For example, if you produce a reusable Bazel module
such as a ruleset, you may want to include the requirements.bzl file rather than make your users
install the WORKSPACE setup to generate it.
See https://github.com/bazelbuild/rules_python/issues/608

This is the same workflow as Gazelle, which creates `go_repository` rules with
[`update-repos`](https://github.com/bazelbuild/bazel-gazelle#update-repos)

To do this, use the "write to source file" pattern documented in
https://blog.aspect.dev/bazel-can-write-to-the-source-folder
to put a copy of the generated requirements.bzl into your project.
Then load the requirements.bzl file directly rather than from the generated repository.
See the example in rules_python/examples/pip_parse_vendored.
""",
    implementation = _pip_repository_impl,
    environ = [
        "RULES_PYTHON_PIP_ISOLATED",
        REPO_DEBUG_ENV_VAR,
    ],
)
