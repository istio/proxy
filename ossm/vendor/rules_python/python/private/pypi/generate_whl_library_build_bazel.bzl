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

"""Generate the BUILD.bazel contents for a repo defined by a whl_library."""

load("//python/private:text_util.bzl", "render")

_RENDER = {
    "copy_executables": render.dict,
    "copy_files": render.dict,
    "data": render.list,
    "data_exclude": render.list,
    "dependencies": render.list,
    "dependencies_by_platform": lambda x: render.dict(x, value_repr = render.list),
    "entry_points": render.dict,
    "extras": render.list,
    "group_deps": render.list,
    "include": str,
    "requires_dist": render.list,
    "srcs_exclude": render.list,
    "tags": render.list,
    "target_platforms": render.list,
}

# NOTE @aignas 2024-10-25: We have to keep this so that files in
# this repository can be publicly visible without the need for
# export_files
_TEMPLATE = """\
{loads}

package(default_visibility = ["//visibility:public"])

{fn}(
{kwargs}
)
"""

def generate_whl_library_build_bazel(
        *,
        annotation = None,
        default_python_version = None,
        **kwargs):
    """Generate a BUILD file for an unzipped Wheel

    Args:
        annotation: The annotation for the build file.
        default_python_version: The python version to use to parse the METADATA.
        **kwargs: Extra args serialized to be passed to the
            {obj}`whl_library_targets`.

    Returns:
        A complete BUILD file as a string
    """

    loads = []
    if kwargs.get("tags"):
        fn = "whl_library_targets"

        # legacy path
        unsupported_args = [
            "requires",
            "metadata_name",
            "metadata_version",
            "packages",
            "include",
        ]
    else:
        fn = "whl_library_targets_from_requires"
        unsupported_args = [
            "dependencies",
            "dependencies_by_platform",
            "target_platforms",
            "default_python_version",
        ]
        packages_load = kwargs.pop("config_load")
        if not kwargs.get("requires_dist"):
            # no deps, we can leave the extra loads out
            pass
        else:
            loads.append("""load("{}", "{}")""".format(packages_load, "packages"))
            kwargs["include"] = "packages"

    for arg in unsupported_args:
        if kwargs.get(arg):
            fail("BUG, unsupported arg: '{}'".format(arg))

    loads.extend([
        """load("@rules_python//python/private/pypi:whl_library_targets.bzl", "{}")""".format(fn),
    ])

    additional_content = []
    entry_points = kwargs.get("entry_points")
    if entry_points:
        entry_point_files = sorted({
            entry_point_script.replace("\\", "/"): True
            for entry_point_script in entry_points.values()
        }.keys())
        additional_content.append(
            "exports_files(\n" +
            "    srcs = {},\n".format(render.list(entry_point_files)) +
            "    visibility = [\"//visibility:public\"],\n" +
            ")\n",
        )
    if annotation:
        kwargs["data"] = annotation.data
        kwargs["copy_files"] = annotation.copy_files
        kwargs["copy_executables"] = annotation.copy_executables
        kwargs["data_exclude"] = kwargs.get("data_exclude", []) + annotation.data_exclude_glob
        kwargs["srcs_exclude"] = annotation.srcs_exclude_glob
        if annotation.additive_build_content:
            additional_content.append(annotation.additive_build_content)
    if default_python_version:
        kwargs["default_python_version"] = default_python_version

    contents = "\n".join(
        [
            _TEMPLATE.format(
                loads = "\n".join(loads),
                fn = fn,
                kwargs = render.indent("\n".join([
                    "{} = {},".format(k, _RENDER.get(k, repr)(v))
                    for k, v in sorted(kwargs.items())
                ])),
            ),
        ] + additional_content,
    )

    # NOTE: Ensure that we terminate with a new line
    return contents.rstrip() + "\n"
