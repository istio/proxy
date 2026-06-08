# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""APIs for operating on universal binaries with `lipo`."""

load(
    "@bazel_skylib//lib:shell.bzl",
    "shell",
)
load(":apple_support.bzl", "apple_support")

def _create(
        *,
        actions,
        inputs,
        output,
        apple_fragment,
        xcode_config):
    """Creates a universal binary by combining other binaries.

    Args:
        actions: The `Actions` object used to register actions.
        inputs: A sequence or `depset` of `File`s that represent binaries to
            be combined. As with the `lipo` tool's `-create` command (when
            invoked without the `-arch` option) all of the architectures in
            each input file will be copied into the output file (so the inputs
            may be either single-architecture binaries or universal binaries).
        output: A `File` representing the universal binary that will be the
            output of the action.
        apple_fragment: The `apple` configuration fragment used to configure
            the action environment.
        xcode_config: The `apple_common.XcodeVersionConfig` provider used to
            configure the action environment.
    """
    if not inputs:
        fail("lipo.create requires at least one input file.")

    # Explicitly create the containing directory to avoid an occasional error
    # from lipo; "can't create temporary output file [...] (Permission denied)"
    command = [
        "mkdir -p {} &&".format(shell.quote(output.dirname)),
        "/usr/bin/lipo",
        "-create",
    ]
    command.extend([
        shell.quote(input_file.path)
        for input_file in inputs
    ])
    command.extend(["-output", shell.quote(output.path)])

    apple_support.run_shell(
        actions = actions,
        command = " ".join(command),
        mnemonic = "AppleLipo",
        inputs = inputs,
        outputs = [output],
        apple_fragment = apple_fragment,
        xcode_config = xcode_config,
    )

def _extract_or_thin(
        *,
        actions,
        apple_fragment,
        archs,
        input_shell_expression = None,
        input_file = None,
        output,
        xcode_config):
    """Extracts a subset of an existing universal binary based on the set of incoming architectures.

    Args:
        actions: The `Actions` object used to register actions.
        apple_fragment: The `apple` configuration fragment used to configure the action environment.
        archs: A list of strings that indicates the exact set of architectures we need to create the
            output binary. As with the `lipo` tool's `-extract` command, all of the selected
            architectures indicated by `archs` in the universal binary will be copied into the
            output file. If only one architecture is selected, this will be rely on the `lipo`
            tool's `-thin` command instead.
        input_shell_expression: A string that represent the universal binary to be extracted. Use
            only if the input cannot be represented adequately as a file, i.e. if `$SDKROOT` is
            required to get at its path. Required if the `input_file` is not set.
        input_file: A `File` that represents the universal binary to be extracted. Required if the
            `input_command` is not set.
        output: A `File` representing the universal binary that will be the output of the action.
        xcode_config: The `apple_common.XcodeVersionConfig` provider used to configure the action
            environment.
    """
    if not input_file and not input_shell_expression:
        fail("lipo.extract_or_thin needs a fat binary in `input_file` or `input_shell_expression`.")
    if input_file and input_shell_expression:
        fail("""
lipo.extract_or_thin cannot use `input_file` along with `input_shell_expression` simultaneously.

Please use only `input_file` or `input_shell_expression` to express the intended input.
""")
    if not archs:
        fail("lipo.extract_or_thin requires at least one arch.")

    # Explicitly create the containing directory if it doesn't exist, otherwise the following error
    # can come from lipo; "can't create temporary output file [...] (Permission denied)"
    command = [
        "mkdir -p {} &&".format(shell.quote(output.dirname)),
        "/usr/bin/lipo",
    ]
    if input_file:
        command.append(shell.quote(input_file.path))
    else:
        command.append(input_shell_expression)
    if len(archs) == 1:
        command.extend(["-thin", archs[0]])
    else:
        for arch in archs:
            command.extend(["-extract", arch])
    command.extend(["-output", shell.quote(output.path)])

    extra_args = {}
    if input_file:
        extra_args["inputs"] = [input_file]
    apple_support.run_shell(
        actions = actions,
        command = " ".join(command),
        mnemonic = "AppleLipoExtract",
        outputs = [output],
        apple_fragment = apple_fragment,
        xcode_config = xcode_config,
        **extra_args
    )

# TODO(apple-rules-team): Add support for other mutating operations here if
# there is a need: extract_family, remove, replace.
lipo = struct(
    create = _create,
    extract_or_thin = _extract_or_thin,
)
