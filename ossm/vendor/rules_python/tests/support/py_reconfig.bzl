# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Run a py_binary/py_test with altered config settings.

This facilitates verify running binaries with different configuration settings
without the overhead of a bazel-in-bazel integration test.
"""

load("//python/private:attr_builders.bzl", "attrb")  # buildifier: disable=bzl-visibility
load("//python/private:py_binary_macro.bzl", "py_binary_macro")  # buildifier: disable=bzl-visibility
load("//python/private:py_binary_rule.bzl", "create_py_binary_rule_builder")  # buildifier: disable=bzl-visibility
load("//python/private:py_test_macro.bzl", "py_test_macro")  # buildifier: disable=bzl-visibility
load("//python/private:py_test_rule.bzl", "create_py_test_rule_builder")  # buildifier: disable=bzl-visibility
load("//tests/support:support.bzl", "VISIBLE_FOR_TESTING")

def _perform_transition_impl(input_settings, attr, base_impl):
    settings = {k: input_settings[k] for k in _RECONFIG_INHERITED_OUTPUTS if k in input_settings}
    settings.update(base_impl(input_settings, attr))

    settings[VISIBLE_FOR_TESTING] = True
    settings["//command_line_option:build_python_zip"] = attr.build_python_zip
    if attr.bootstrap_impl:
        settings["//python/config_settings:bootstrap_impl"] = attr.bootstrap_impl
    if attr.extra_toolchains:
        settings["//command_line_option:extra_toolchains"] = attr.extra_toolchains
    if attr.python_src:
        settings["//python/bin:python_src"] = attr.python_src
    if attr.repl_dep:
        settings["//python/bin:repl_dep"] = attr.repl_dep
    if attr.venvs_use_declare_symlink:
        settings["//python/config_settings:venvs_use_declare_symlink"] = attr.venvs_use_declare_symlink
    if attr.venvs_site_packages:
        settings["//python/config_settings:venvs_site_packages"] = attr.venvs_site_packages
    return settings

_RECONFIG_INPUTS = [
    "//python/config_settings:bootstrap_impl",
    "//python/bin:python_src",
    "//python/bin:repl_dep",
    "//command_line_option:extra_toolchains",
    "//python/config_settings:venvs_use_declare_symlink",
    "//python/config_settings:venvs_site_packages",
]
_RECONFIG_OUTPUTS = _RECONFIG_INPUTS + [
    "//command_line_option:build_python_zip",
    VISIBLE_FOR_TESTING,
]
_RECONFIG_INHERITED_OUTPUTS = [v for v in _RECONFIG_OUTPUTS if v in _RECONFIG_INPUTS]

_RECONFIG_ATTRS = {
    "bootstrap_impl": attrb.String(),
    "build_python_zip": attrb.String(default = "auto"),
    "extra_toolchains": attrb.StringList(
        doc = """
Value for the --extra_toolchains flag.

NOTE: You'll likely have to also specify //tests/support/cc_toolchains:all (or some CC toolchain)
to make the RBE presubmits happy, which disable auto-detection of a CC
toolchain.
""",
    ),
    "python_src": attrb.Label(),
    "repl_dep": attrb.Label(),
    "venvs_site_packages": attrb.String(),
    "venvs_use_declare_symlink": attrb.String(),
}

def _create_reconfig_rule(builder):
    builder.attrs.update(_RECONFIG_ATTRS)

    base_cfg_impl = builder.cfg.implementation()
    builder.cfg.set_implementation(lambda *args: _perform_transition_impl(base_impl = base_cfg_impl, *args))
    builder.cfg.update_inputs(_RECONFIG_INPUTS)
    builder.cfg.update_outputs(_RECONFIG_OUTPUTS)
    return builder.build()

_py_reconfig_binary = _create_reconfig_rule(create_py_binary_rule_builder())

_py_reconfig_test = _create_reconfig_rule(create_py_test_rule_builder())

def py_reconfig_test(**kwargs):
    """Create a py_test with customized build settings for testing.

    Args:
        **kwargs: kwargs to pass along to _py_reconfig_test.
    """
    py_test_macro(_py_reconfig_test, **kwargs)

def py_reconfig_binary(**kwargs):
    py_binary_macro(_py_reconfig_binary, **kwargs)
