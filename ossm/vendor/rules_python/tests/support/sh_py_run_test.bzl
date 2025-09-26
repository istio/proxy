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
"""Run a py_binary with altered config settings in an sh_test.

This facilitates verify running binaries with different configuration settings
without the overhead of a bazel-in-bazel integration test.
"""

load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//python/private:attr_builders.bzl", "attrb")  # buildifier: disable=bzl-visibility
load("//python/private:py_binary_macro.bzl", "py_binary_macro")  # buildifier: disable=bzl-visibility
load("//python/private:py_binary_rule.bzl", "create_py_binary_rule_builder")  # buildifier: disable=bzl-visibility
load("//python/private:py_test_macro.bzl", "py_test_macro")  # buildifier: disable=bzl-visibility
load("//python/private:py_test_rule.bzl", "create_py_test_rule_builder")  # buildifier: disable=bzl-visibility
load("//python/private:toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility
load("//tests/support:support.bzl", "VISIBLE_FOR_TESTING")

def _perform_transition_impl(input_settings, attr, base_impl):
    settings = {k: input_settings[k] for k in _RECONFIG_INHERITED_OUTPUTS if k in input_settings}
    settings.update(base_impl(input_settings, attr))

    settings[VISIBLE_FOR_TESTING] = True
    settings["//command_line_option:build_python_zip"] = attr.build_python_zip

    for attr_name, setting_label in _RECONFIG_ATTR_SETTING_MAP.items():
        if getattr(attr, attr_name):
            settings[setting_label] = getattr(attr, attr_name)
    return settings

# Attributes that, if non-falsey (`if attr.<name>`), will copy their
# value into the output settings
_RECONFIG_ATTR_SETTING_MAP = {
    "bootstrap_impl": "//python/config_settings:bootstrap_impl",
    "custom_runtime": "//tests/support:custom_runtime",
    "extra_toolchains": "//command_line_option:extra_toolchains",
    "python_src": "//python/bin:python_src",
    "venvs_site_packages": "//python/config_settings:venvs_site_packages",
    "venvs_use_declare_symlink": "//python/config_settings:venvs_use_declare_symlink",
}

_RECONFIG_INPUTS = _RECONFIG_ATTR_SETTING_MAP.values()
_RECONFIG_OUTPUTS = _RECONFIG_INPUTS + [
    "//command_line_option:build_python_zip",
    VISIBLE_FOR_TESTING,
]
_RECONFIG_INHERITED_OUTPUTS = [v for v in _RECONFIG_OUTPUTS if v in _RECONFIG_INPUTS]

_RECONFIG_ATTRS = {
    "bootstrap_impl": attrb.String(),
    "build_python_zip": attrb.String(default = "auto"),
    "custom_runtime": attrb.String(),
    "extra_toolchains": attrb.StringList(
        doc = """
Value for the --extra_toolchains flag.

NOTE: You'll likely have to also specify //tests/support/cc_toolchains:all (or some CC toolchain)
to make the RBE presubmits happy, which disable auto-detection of a CC
toolchain.
""",
    ),
    "python_src": attrb.Label(),
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

def sh_py_run_test(*, name, sh_src, py_src, **kwargs):
    """Run a py_binary within a sh_test.

    Args:
        name: name of the sh_test and base name of inner targets.
        sh_src: .sh file to run as a test
        py_src: .py file for the py_binary
        **kwargs: additional kwargs passed onto py_binary and/or sh_test
    """
    bin_name = "_{}_bin".format(name)
    sh_test(
        name = name,
        srcs = [sh_src],
        data = [bin_name],
        deps = [
            "@bazel_tools//tools/bash/runfiles",
        ],
        env = {
            "BIN_RLOCATION": "$(rlocationpaths {})".format(bin_name),
        },
    )
    py_reconfig_binary(
        name = bin_name,
        srcs = [py_src],
        main = py_src,
        tags = ["manual"],
        **kwargs
    )

def _current_build_settings_impl(ctx):
    info = ctx.actions.declare_file(ctx.label.name + ".json")
    toolchain = ctx.toolchains[TARGET_TOOLCHAIN_TYPE]
    runtime = toolchain.py3_runtime
    files = [info]
    ctx.actions.write(
        output = info,
        content = json.encode({
            "bootstrap_impl": ctx.attr._bootstrap_impl_flag[config_common.FeatureFlagInfo].value,
            "interpreter": {
                "short_path": runtime.interpreter.short_path if runtime.interpreter else None,
            },
            "interpreter_path": runtime.interpreter_path,
            "toolchain_label": str(getattr(toolchain, "toolchain_label", None)),
        }),
    )
    return [DefaultInfo(
        files = depset(files),
    )]

current_build_settings = rule(
    doc = """
Writes information about the current build config to JSON for testing.

This is so tests can verify information about the build config used for them.
""",
    implementation = _current_build_settings_impl,
    attrs = {
        "_bootstrap_impl_flag": attr.label(
            default = "//python/config_settings:bootstrap_impl",
        ),
    },
    toolchains = [
        TARGET_TOOLCHAIN_TYPE,
    ],
)
