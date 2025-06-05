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

load("//python/private:text_util.bzl", "render")
load(":render_pkg_aliases.bzl", "render_multiplatform_pkg_aliases")
load(":whl_config_setting.bzl", "whl_config_setting")

_BUILD_FILE_CONTENTS = """\
package(default_visibility = ["//visibility:public"])

# Ensure the `requirements.bzl` source can be accessed by stardoc, since users load() from it
exports_files(["requirements.bzl"])
"""

def _impl(rctx):
    bzl_packages = rctx.attr.packages or rctx.attr.whl_map.keys()
    aliases = render_multiplatform_pkg_aliases(
        aliases = {
            key: _whl_config_settings_from_json(values)
            for key, values in rctx.attr.whl_map.items()
        },
        extra_hub_aliases = rctx.attr.extra_hub_aliases,
        requirement_cycles = rctx.attr.groups,
    )
    for path, contents in aliases.items():
        rctx.file(path, contents)

    # NOTE: we are using the canonical name with the double '@' in order to
    # always uniquely identify a repository, as the labels are being passed as
    # a string and the resolution of the label happens at the call-site of the
    # `requirement`, et al. macros.
    macro_tmpl = "@@{name}//{{}}:{{}}".format(name = rctx.attr.name)

    rctx.file("BUILD.bazel", _BUILD_FILE_CONTENTS)
    rctx.template("requirements.bzl", rctx.attr._template, substitutions = {
        "%%ALL_DATA_REQUIREMENTS%%": render.list([
            macro_tmpl.format(p, "data")
            for p in bzl_packages
        ]),
        "%%ALL_REQUIREMENTS%%": render.list([
            macro_tmpl.format(p, "pkg")
            for p in bzl_packages
        ]),
        "%%ALL_WHL_REQUIREMENTS_BY_PACKAGE%%": render.dict({
            p: macro_tmpl.format(p, "whl")
            for p in bzl_packages
        }),
        "%%MACRO_TMPL%%": macro_tmpl,
    })

hub_repository = repository_rule(
    attrs = {
        "extra_hub_aliases": attr.string_list_dict(
            doc = "Extra aliases to make for specific wheels in the hub repo.",
            mandatory = True,
        ),
        "groups": attr.string_list_dict(
            mandatory = False,
        ),
        "packages": attr.string_list(
            mandatory = False,
            doc = """\
The list of packages that will be exposed via all_*requirements macros. Defaults to whl_map keys.
""",
        ),
        "repo_name": attr.string(
            mandatory = True,
            doc = "The apparent name of the repo. This is needed because in bzlmod, the name attribute becomes the canonical name.",
        ),
        "whl_map": attr.string_dict(
            mandatory = True,
            doc = """\
The wheel map where values are json.encoded strings of the whl_map constructed
in the pip.parse tag class.
""",
        ),
        "_template": attr.label(
            default = ":requirements.bzl.tmpl.bzlmod",
        ),
    },
    doc = """A rule for bzlmod mulitple pip repository creation. PRIVATE USE ONLY.""",
    implementation = _impl,
)

def _whl_config_settings_from_json(repo_mapping_json):
    """Deserialize the serialized values with whl_config_settings_to_json.

    Args:
        repo_mapping_json: {type}`str`

    Returns:
        What `whl_config_settings_to_json` accepts.
    """
    return {
        whl_config_setting(**v): repo
        for repo, values in json.decode(repo_mapping_json).items()
        for v in values
    }

def whl_config_settings_to_json(repo_mapping):
    """A function to serialize the aliases so that `hub_repository` can accept them.

    Args:
        repo_mapping: {type}`dict[str, list[struct]]` repo to
            {obj}`whl_config_setting` mapping.

    Returns:
        A deserializable JSON string
    """
    return json.encode({
        repo: [_whl_config_setting_dict(s) for s in settings]
        for repo, settings in repo_mapping.items()
    })

def _whl_config_setting_dict(a):
    ret = {}
    if a.config_setting:
        ret["config_setting"] = a.config_setting
    if a.filename:
        ret["filename"] = a.filename
    if a.target_platforms:
        ret["target_platforms"] = a.target_platforms
    if a.version:
        ret["version"] = a.version
    return ret
