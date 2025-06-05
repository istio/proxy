# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Helper functions to expand paths into runfiles
"""

# Expand $(rootpath) and $(rootpaths) to runfiles manifest path.
# Runfiles manifest path is of the form:
# - repo/path/to/file
def _expand_rootpath_to_manifest_path(ctx, input, targets):
    paths = ctx.expand_location(input, targets)
    return " ".join([_rootpath_to_runfiles_manifest_path(ctx, p, targets) for p in paths.split(" ")])

# Convert an runfiles rootpath to a runfiles manifestpath.
# Runfiles rootpath is returned from ctx.expand_location $(rootpath) and $(rootpaths):
# - ./file
# - path/to/file
# - ../external_repo/path/to/file
# This is converted to the runfiles manifest path of:
# - repo/path/to/file
def _rootpath_to_runfiles_manifest_path(ctx, path, targets):
    if path.startswith("../"):
        return path[len("../"):]
    if path.startswith("./"):
        path = path[len("./"):]
    return ctx.workspace_name + "/" + path

def expand_location_into_runfiles(ctx, input, targets = []):
    """Expands all `$(execpath ...)`, `$(rootpath ...)` and legacy `$(location ...)` templates in the
    given string by replacing with the expanded path. Expansion only works for labels that point to direct dependencies
    of this rule or that are explicitly listed in the optional argument targets.

    See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.

    Use `$(rootpath)` and `$(rootpaths)` to expand labels to the runfiles path that a built binary can use
    to find its dependencies. This path is of the format:
    - `./file`
    - `path/to/file`
    - `../external_repo/path/to/file`

    Use `$(execpath)` and `$(execpaths)` to expand labels to the execroot (where Bazel runs build actions).
    This is of the format:
    - `./file`
    - `path/to/file`
    - `external/external_repo/path/to/file`
    - `<bin_dir>/path/to/file`
    - `<bin_dir>/external/external_repo/path/to/file`

    The legacy `$(location)` and `$(locations)` expansion is DEPRECATED as it returns the runfiles manifest path of the
    format `repo/path/to/file` which behaves differently than the built-in `$(location)` expansion in args of *_binary
    and *_test rules which returns the rootpath.
    See https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes-binaries.

    The legacy `$(location)` and `$(locations)` expansion also differs from how the builtin `ctx.expand_location()` expansions
    of `$(location)` and `$(locations)` behave as that function returns either the execpath or rootpath depending on the context.
    See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.

    The behavior of `$(location)` and `$(locations)` expansion will be fixed in a future major release to match the
    to default Bazel behavior and return the same path as `ctx.expand_location()` returns for these.

    The recommended approach is to now use `$(rootpath)` where you previously used $(location). See the docstrings
    of `nodejs_binary` or `params_file` for examples of how to use `$(rootpath)` in `templated_args` and `args` respectively.

    Args:
      ctx: context
      input: String to be expanded
      targets: List of targets for additional lookup information.

    Returns:
      The expanded path or the original path
    """
    target = "@%s//%s:%s" % (ctx.workspace_name, "/".join(ctx.build_file_path.split("/")[:-1]), ctx.attr.name)

    # Loop through input an expand all predefined source/output path variables
    # See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.
    path = ""
    length = len(input)
    last = 0
    for i in range(length):
        # Support legacy $(location) and $(locations) expansions which return the runfiles manifest path
        # in the format `repo/path/to/file`. This expansion is DEPRECATED. See docstring above.
        # TODO: Change location to behave the same as the built-in $(location) expansion for args of *_binary
        #       and *_test rules. This would be a BREAKING CHANGE.
        if input[i:].startswith("$(location ") or input[i:].startswith("$(locations "):
            j = input.find(")", i) + 1
            if (j == 0):
                fail("invalid \"%s\" expansion in string \"%s\" part of target %s" % (input[i:j], input, target))
            path += input[last:i]
            path += _expand_rootpath_to_manifest_path(ctx, "$(rootpath" + input[i + 10:j], targets)
            last = j
            i = j

        # Expand $(execpath) $(execpaths) $(rootpath) $(rootpaths) with plain ctx.expand_location()
        if input[i:].startswith("$(execpath ") or input[i:].startswith("$(execpaths ") or input[i:].startswith("$(rootpath ") or input[i:].startswith("$(rootpaths "):
            j = input.find(")", i) + 1
            if (j == 0):
                fail("invalid \"%s\" expansion in string \"%s\" part of target %s" % (input[i:j], input, target))
            path += input[last:i]
            path += ctx.expand_location(input[i:j], targets)
            last = j
            i = j
    path += input[last:]

    return path
