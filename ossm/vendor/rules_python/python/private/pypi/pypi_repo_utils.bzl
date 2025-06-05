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

""

load("@bazel_skylib//lib:types.bzl", "types")
load("//python/private:repo_utils.bzl", "repo_utils")

def _get_python_interpreter_attr(mrctx, *, python_interpreter = None):
    """A helper function for getting the `python_interpreter` attribute or it's default

    Args:
        mrctx (module_ctx or repository_ctx): Handle to the rule repository context.
        python_interpreter (str): The python interpreter override.

    Returns:
        str: The attribute value or it's default
    """
    if python_interpreter:
        return python_interpreter

    os = repo_utils.get_platforms_os_name(mrctx)
    if "windows" in os:
        return "python.exe"
    else:
        return "python3"

def _resolve_python_interpreter(mrctx, *, python_interpreter = None, python_interpreter_target = None):
    """Helper function to find the python interpreter from the common attributes

    Args:
        mrctx: Handle to the module_ctx or repository_ctx.
        python_interpreter: str, the python interpreter to use.
        python_interpreter_target: Label, the python interpreter to use after
            downloading the label.

    Returns:
        `path` object, for the resolved path to the Python interpreter.
    """
    python_interpreter = _get_python_interpreter_attr(mrctx, python_interpreter = python_interpreter)

    if python_interpreter_target != None:
        # The following line would make the MODULE.bazel.lock platform
        # independent, because the lock file will then contain a hash of the
        # file so that the lock file can be recalculated, hence the best way is
        # to add this directory to PATH.
        #
        # hence we add the root BUILD.bazel file and get the directory of that
        # and construct the path differently. At the end of the day we don't
        # want the hash of the interpreter to end up in the lock file.
        if hasattr(python_interpreter_target, "same_package_label"):
            root_build_bazel = python_interpreter_target.same_package_label("BUILD.bazel")
        else:
            root_build_bazel = python_interpreter_target.relative(":BUILD.bazel")

        python_interpreter = mrctx.path(root_build_bazel).dirname.get_child(python_interpreter_target.name)

        os = repo_utils.get_platforms_os_name(mrctx)

        # On Windows, the symlink doesn't work because Windows attempts to find
        # Python DLLs where the symlink is, not where the symlink points.
        if "windows" in os:
            python_interpreter = python_interpreter.realpath
    elif "/" not in python_interpreter:
        # It's a plain command, e.g. "python3", to look up in the environment.
        python_interpreter = repo_utils.which_checked(mrctx, python_interpreter)
    else:
        python_interpreter = mrctx.path(python_interpreter)
    return python_interpreter

def _construct_pypath(mrctx, *, entries):
    """Helper function to construct a PYTHONPATH.

    Contains entries for code in this repo as well as packages downloaded from //python/pip_install:repositories.bzl.
    This allows us to run python code inside repository rule implementations.

    Args:
        mrctx: Handle to the module_ctx or repository_ctx.
        entries: The list of entries to add to PYTHONPATH.

    Returns: String of the PYTHONPATH.
    """

    if not entries:
        return None

    os = repo_utils.get_platforms_os_name(mrctx)
    separator = ";" if "windows" in os else ":"
    pypath = separator.join([
        str(mrctx.path(entry).dirname)
        # Use a dict as a way to remove duplicates and then sort it.
        for entry in sorted({x: None for x in entries})
    ])
    return pypath

def _execute_checked(mrctx, *, srcs, **kwargs):
    """Helper function to run a python script and modify the PYTHONPATH to include external deps.

    Args:
        mrctx: Handle to the module_ctx or repository_ctx.
        srcs: The src files that the script depends on. This is important to
            ensure that the Bazel repository cache or the bzlmod lock file gets
            invalidated when any one file changes. It is advisable to use
            `RECORD` files for external deps and the list of srcs from the
            rules_python repo for any scripts.
        **kwargs: Arguments forwarded to `repo_utils.execute_checked`. If
            the `environment` has a value `PYTHONPATH` and it is a list, then
            it will be passed to `construct_pythonpath` function.
    """

    for src in srcs:
        # This will ensure that we will re-evaluate the bzlmod extension or
        # refetch the repository_rule when the srcs change. This should work on
        # Bazel versions without `mrctx.watch` as well.
        repo_utils.watch(mrctx, mrctx.path(src))

    env = kwargs.pop("environment", {})
    pythonpath = env.get("PYTHONPATH", "")
    if pythonpath and not types.is_string(pythonpath):
        env["PYTHONPATH"] = _construct_pypath(mrctx, entries = pythonpath)

    return repo_utils.execute_checked(
        mrctx,
        environment = env,
        **kwargs
    )

pypi_repo_utils = struct(
    construct_pythonpath = _construct_pypath,
    execute_checked = _execute_checked,
    resolve_python_interpreter = _resolve_python_interpreter,
)
