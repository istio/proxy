# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""A simple function that evaluates markers using a python interpreter."""

load(":deps.bzl", "record_files")
load(":pypi_repo_utils.bzl", "pypi_repo_utils")

# Used as a default value in a rule to ensure we fetch the dependencies.
SRCS = [
    # When the version, or any of the files in `packaging` package changes,
    # this file will change as well.
    record_files["pypi__packaging"],
    Label("//python/private/pypi/requirements_parser:resolve_target_platforms.py"),
    Label("//python/private/pypi/whl_installer:platform.py"),
]

def evaluate_markers(mrctx, *, requirements, python_interpreter, python_interpreter_target, srcs, logger = None):
    """Return the list of supported platforms per requirements line.

    Args:
        mrctx: repository_ctx or module_ctx.
        requirements: list[str] of the requirement file lines to evaluate.
        python_interpreter: str, path to the python_interpreter to use to
            evaluate the env markers in the given requirements files. It will
            be only called if the requirements files have env markers. This
            should be something that is in your PATH or an absolute path.
        python_interpreter_target: Label, same as python_interpreter, but in a
            label format.
        srcs: list[Label], the value of SRCS passed from the `rctx` or `mctx` to this function.
        logger: repo_utils.logger or None, a simple struct to log diagnostic
            messages. Defaults to None.

    Returns:
        dict of string lists with target platforms
    """
    if not requirements:
        return {}

    in_file = mrctx.path("requirements_with_markers.in.json")
    out_file = mrctx.path("requirements_with_markers.out.json")
    mrctx.file(in_file, json.encode(requirements))

    pypi_repo_utils.execute_checked(
        mrctx,
        op = "ResolveRequirementEnvMarkers({})".format(in_file),
        arguments = [
            pypi_repo_utils.resolve_python_interpreter(
                mrctx,
                python_interpreter = python_interpreter,
                python_interpreter_target = python_interpreter_target,
            ),
            "-m",
            "python.private.pypi.requirements_parser.resolve_target_platforms",
            in_file,
            out_file,
        ],
        srcs = srcs,
        environment = {
            "PYTHONPATH": [
                Label("@pypi__packaging//:BUILD.bazel"),
                Label("//:BUILD.bazel"),
            ],
        },
        logger = logger,
    )
    return json.decode(mrctx.read(out_file))
