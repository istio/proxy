# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Test rule to perform generic bundle verification tests."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")

_supports_visionos = hasattr(apple_common.platform_type, "visionos")

def _transition_impl(_, attr):
    output_dictionary = {
        "//command_line_option:apple_generate_dsym": attr.generate_dsym,
        "//command_line_option:compilation_mode": attr.compilation_mode,
        "//command_line_option:cpu": "darwin_x86_64",
        "//command_line_option:ios_signing_cert_name": "-",
        "//command_line_option:macos_cpus": "x86_64",
        "//command_line_option:objc_enable_binary_stripping": attr.objc_enable_binary_stripping,
    }
    if attr.build_type == "simulator":
        output_dictionary.update({
            "//command_line_option:ios_multi_cpus": "x86_64",
            "//command_line_option:tvos_cpus": "x86_64",
            "//command_line_option:visionos_cpus": "x86_64",
            "//command_line_option:watchos_cpus": "x86_64",
        })

    else:
        output_dictionary.update({
            "//command_line_option:ios_multi_cpus": "arm64",
            "//command_line_option:tvos_cpus": "arm64",
            "//command_line_option:visionos_cpus": "arm64",
            "//command_line_option:watchos_cpus": "arm64_32,armv7k",
        })

    if hasattr(attr, "cpus"):
        for cpu_option, cpu in attr.cpus.items():
            command_line_option = "//command_line_option:%s" % cpu_option
            output_dictionary.update({command_line_option: cpu})

    if not _supports_visionos:
        output_dictionary.pop("//command_line_option:visionos_cpus", None)

    return output_dictionary

_transition = transition(
    implementation = _transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:apple_generate_dsym",
        "//command_line_option:compilation_mode",
        "//command_line_option:cpu",
        "//command_line_option:ios_multi_cpus",
        "//command_line_option:ios_signing_cert_name",
        "//command_line_option:macos_cpus",
        "//command_line_option:objc_enable_binary_stripping",
        "//command_line_option:tvos_cpus",
        "//command_line_option:watchos_cpus",
    ] + (["//command_line_option:visionos_cpus"] if _supports_visionos else []),
)

def _apple_verification_test_impl(ctx):
    binary = ctx.attr.target_under_test[0].files.to_list()[0]
    output_script = ctx.actions.declare_file("{}_test_script".format(ctx.label.name))
    ctx.actions.expand_template(
        template = ctx.file.verifier_script,
        output = output_script,
        substitutions = {
            "%{binary}s": binary.short_path,
        },
        is_executable = True,
    )

    # Extra test environment to set during the test.
    test_env = {
        "BUILD_TYPE": ctx.attr.build_type,
        "PLATFORM_TYPE": ctx.attr.expected_platform_type,
    }

    if ctx.attr.cpus:
        cpu = ctx.attr.cpus.values()[0]
        if cpu.startswith("sim_"):
            cpu = cpu[4:]
        test_env["CPU"] = cpu

    xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig]

    return [
        testing.ExecutionInfo(xcode_config.execution_info()),
        testing.TestEnvironment(dicts.add(
            apple_common.apple_host_system_env(xcode_config),
            test_env,
        )),
        DefaultInfo(
            executable = output_script,
            runfiles = ctx.runfiles(
                files = [binary],
            ),
        ),
    ]

apple_verification_test = rule(
    implementation = _apple_verification_test_impl,
    attrs = {
        "generate_dsym": attr.bool(
            default = False,
            doc = """
Whether to generate a dSYM file for the binary under test.
""",
        ),
        "build_type": attr.string(
            mandatory = True,
            values = ["simulator", "device"],
            doc = """
Type of build for the target under test. Possible values are `simulator` or `device`.
""",
        ),
        "compilation_mode": attr.string(
            values = ["fastbuild", "opt", "dbg"],
            doc = """
Possible values are `fastbuild`, `dbg` or `opt`. Defaults to `fastbuild`.
https://docs.bazel.build/versions/master/user-manual.html#flag--compilation_mode
""",
            default = "fastbuild",
        ),
        "cpus": attr.string_dict(
            doc = """
Dictionary of command line options cpu flags and the list of
cpu's to use for test under target (e.g. {'ios_multi_cpus': ['arm64', 'x86_64']})
""",
        ),
        "expected_platform_type": attr.string(
            default = "",
            doc = """
The apple_platform_type the binary should have been built for.
""",
        ),
        "objc_enable_binary_stripping": attr.bool(
            default = False,
            doc = """
Whether to perform symbol and dead-code strippings on linked binaries. Binary
strippings will be performed if both this flag and --compilation_mode=opt are
specified.
""",
        ),
        "target_under_test": attr.label(
            mandatory = True,
            doc = "The binary being verified.",
            cfg = _transition,
        ),
        "verifier_script": attr.label(
            mandatory = True,
            allow_single_file = [".sh"],
            doc = """
Script containing the verification code.
""",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_xcode_config": attr.label(
            default = configuration_field(
                name = "xcode_config_label",
                fragment = "apple",
            ),
        ),
    },
    test = True,
    fragments = ["apple"],
)
