# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Public API for for building wheels."""

load("@bazel_skylib//rules:native_binary.bzl", "native_binary")
load("//python:py_binary.bzl", "py_binary")
load("//python/private:bzlmod_enabled.bzl", "BZLMOD_ENABLED")
load("//python/private:py_package.bzl", "py_package_lib")
load("//python/private:py_wheel.bzl", _PyWheelInfo = "PyWheelInfo", _py_wheel = "py_wheel")
load("//python/private:util.bzl", "copy_propagating_kwargs")

# Re-export as public API
PyWheelInfo = _PyWheelInfo

py_package = rule(
    implementation = py_package_lib.implementation,
    doc = """\
A rule to select all files in transitive dependencies of deps which
belong to given set of Python packages.

This rule is intended to be used as data dependency to py_wheel rule.
""",
    attrs = py_package_lib.attrs,
)

def _py_wheel_dist_impl(ctx):
    out = ctx.actions.declare_directory(ctx.attr.out)
    name_file = ctx.attr.wheel[PyWheelInfo].name_file
    wheel = ctx.attr.wheel[PyWheelInfo].wheel

    args = ctx.actions.args()
    args.add("--wheel", wheel)
    args.add("--name_file", name_file)
    args.add("--output", out.path)

    ctx.actions.run(
        mnemonic = "PyWheelDistDir",
        executable = ctx.executable._copier,
        inputs = [wheel, name_file],
        outputs = [out],
        arguments = [args],
    )
    return [
        DefaultInfo(
            files = depset([out]),
            runfiles = ctx.runfiles([out]),
        ),
    ]

py_wheel_dist = rule(
    doc = """\
Prepare a dist/ folder, following Python's packaging standard practice.

See https://packaging.python.org/en/latest/tutorials/packaging-projects/#generating-distribution-archives
which recommends a dist/ folder containing the wheel file(s), source distributions, etc.

This also has the advantage that stamping information is included in the wheel's filename.
""",
    implementation = _py_wheel_dist_impl,
    attrs = {
        "out": attr.string(
            doc = "name of the resulting directory",
            mandatory = True,
        ),
        "wheel": attr.label(
            doc = "a [py_wheel target](#py_wheel)",
            providers = [PyWheelInfo],
        ),
        "_copier": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//python/private:py_wheel_dist"),
        ),
    },
)

def py_wheel(
        name,
        twine = None,
        twine_binary = Label("//tools/publish:twine") if BZLMOD_ENABLED else None,
        publish_args = [],
        **kwargs):
    """Builds a Python Wheel.

    Wheels are Python distribution format defined in https://www.python.org/dev/peps/pep-0427/.

    This macro packages a set of targets into a single wheel.
    It wraps the [py_wheel rule](#py_wheel_rule).

    Currently only pure-python wheels are supported.

    Examples:

    ```python
    # Package some specific py_library targets, without their dependencies
    py_wheel(
        name = "minimal_with_py_library",
        # Package data. We're building "example_minimal_library-0.0.1-py3-none-any.whl"
        distribution = "example_minimal_library",
        python_tag = "py3",
        version = "0.0.1",
        deps = [
            "//examples/wheel/lib:module_with_data",
            "//examples/wheel/lib:simple_module",
        ],
    )

    # Use py_package to collect all transitive dependencies of a target,
    # selecting just the files within a specific python package.
    py_package(
        name = "example_pkg",
        # Only include these Python packages.
        packages = ["examples.wheel"],
        deps = [":main"],
    )

    py_wheel(
        name = "minimal_with_py_package",
        # Package data. We're building "example_minimal_package-0.0.1-py3-none-any.whl"
        distribution = "example_minimal_package",
        python_tag = "py3",
        version = "0.0.1",
        deps = [":example_pkg"],
    )
    ```

    To publish the wheel to PyPI, the twine package is required and it is installed
    by default on `bzlmod` setups. On legacy `WORKSPACE`, `rules_python`
    doesn't provide `twine` itself
    (see https://github.com/bazelbuild/rules_python/issues/1016), but
    you can install it with `pip_parse`, just like we do any other dependencies.

    Once you've installed twine, you can pass its label to the `twine`
    attribute of this macro, to get a "[name].publish" target.

    Example:

    ```python
    py_wheel(
        name = "my_wheel",
        twine = "@publish_deps//twine",
        ...
    )
    ```

    Now you can run a command like the following, which publishes to https://test.pypi.org/

    ```sh
    % TWINE_USERNAME=__token__ TWINE_PASSWORD=pypi-*** \\
        bazel run --stamp --embed_label=1.2.4 -- \\
        //path/to:my_wheel.publish --repository testpypi
    ```

    Args:
        name:  A unique name for this target.
        twine: A label of the external location of the py_library target for twine
        twine_binary: A label of the external location of a binary target for twine.
        publish_args: arguments passed to twine, e.g. ["--repository-url", "https://pypi.my.org/simple/"].
            These are subject to make var expansion, as with the `args` attribute.
            Note that you can also pass additional args to the bazel run command as in the example above.
        **kwargs: other named parameters passed to the underlying [py_wheel rule](#py_wheel_rule)
    """
    tags = kwargs.pop("tags", [])
    manual_tags = depset(tags + ["manual"]).to_list()

    dist_target = "{}.dist".format(name)
    py_wheel_dist(
        name = dist_target,
        wheel = name,
        out = kwargs.pop("dist_folder", "{}_dist".format(name)),
        tags = manual_tags,
        **copy_propagating_kwargs(kwargs)
    )

    _py_wheel(
        name = name,
        tags = tags,
        **kwargs
    )

    twine_args = []
    if twine or twine_binary:
        twine_args = ["upload"]
        twine_args.extend(publish_args)
        twine_args.append("$(rootpath :{})/*".format(dist_target))

    if twine_binary:
        native_binary(
            name = "{}.publish".format(name),
            src = twine_binary,
            out = select({
                "@platforms//os:windows": "{}.publish_script.exe".format(name),
                "//conditions:default": "{}.publish_script".format(name),
            }),
            args = twine_args,
            data = [dist_target],
            tags = manual_tags,
            visibility = kwargs.get("visibility"),
            **copy_propagating_kwargs(kwargs)
        )
    elif twine:
        if not twine.endswith(":pkg"):
            fail("twine label should look like @my_twine_repo//:pkg")

        twine_main = twine.replace(":pkg", ":rules_python_wheel_entry_point_twine.py")

        py_binary(
            name = "{}.publish".format(name),
            srcs = [twine_main],
            args = twine_args,
            data = [dist_target],
            imports = ["."],
            main = twine_main,
            deps = [twine],
            tags = manual_tags,
            visibility = kwargs.get("visibility"),
            **copy_propagating_kwargs(kwargs)
        )

py_wheel_rule = _py_wheel
