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

"""IBTool related actions."""

load(
    "@bazel_skylib//lib:collections.bzl",
    "collections",
)
load(
    "@bazel_skylib//lib:paths.bzl",
    "paths",
)
load(
    "@build_bazel_apple_support//lib:apple_support.bzl",
    "apple_support",
)
load(
    "//apple/internal/utils:xctoolrunner.bzl",
    xctoolrunner_support = "xctoolrunner",
)

def _ibtool_arguments(min_os, families):
    """Returns common `ibtool` command line arguments.

    This function returns the common arguments used by both xib and storyboard
    compilation, as well as storyboard linking. Callers should add their own
    arguments to the returned array for their specific purposes.

    Args:
      min_os: The minimum OS version to use when compiling interface files.
      families: The families that should be supported by the compiled interfaces.

    Returns:
      An array of command-line arguments to pass to ibtool.
    """
    return [
        "--minimum-deployment-target",
        min_os,
    ] + collections.before_each(
        "--target-device",
        families,
    )

def compile_storyboard(
        *,
        actions,
        input_file,
        output_dir,
        platform_prerequisites,
        swift_module,
        xctoolrunner):
    """Creates an action that compiles a storyboard.

    Args:
      actions: The actions provider from `ctx.actions`.
      input_file: The storyboard to compile.
      output_dir: The directory where the compiled outputs should be placed.
      platform_prerequisites: Struct containing information on the platform being targeted.
      swift_module: The name of the Swift module to use when compiling the
        storyboard.
      xctoolrunner: A files_to_run for the wrapper around the "xcrun" tool.
    """

    args = [
        "ibtool",
        "--compilation-directory",
        xctoolrunner_support.prefixed_path(output_dir.dirname),
    ]

    min_os = platform_prerequisites.minimum_os
    families = platform_prerequisites.device_families
    args.extend(_ibtool_arguments(min_os, families))
    args.extend([
        "--module",
        swift_module,
        xctoolrunner_support.prefixed_path(input_file.path),
    ])

    apple_support.run(
        actions = actions,
        arguments = args,
        apple_fragment = platform_prerequisites.apple_fragment,
        executable = xctoolrunner,
        execution_requirements = {"no-sandbox": "1"},
        inputs = [input_file],
        mnemonic = "StoryboardCompile",
        outputs = [output_dir],
        xcode_config = platform_prerequisites.xcode_version_config,
    )

def link_storyboards(
        *,
        actions,
        output_dir,
        platform_prerequisites,
        storyboardc_dirs,
        xctoolrunner):
    """Creates an action that links multiple compiled storyboards.

    Storyboards that reference each other must be linked, and this operation also
    copies them into a directory structure matching that which should appear in
    the final bundle.

    Args:
      actions: The actions provider from `ctx.actions`.
      output_dir: The directory where the linked outputs should be placed.
      platform_prerequisites: Struct containing information on the platform being targeted.
      storyboardc_dirs: A list of `File`s that represent directories containing
        the compiled storyboards.
      xctoolrunner: A files_to_run for the wrapper for the "xcrun" tools.
    """

    min_os = platform_prerequisites.minimum_os
    families = platform_prerequisites.device_families

    args = [
        "ibtool",
        "--link",
        xctoolrunner_support.prefixed_path(output_dir.path),
    ]
    args.extend(_ibtool_arguments(min_os, families))
    args.extend([
        xctoolrunner_support.prefixed_path(f.path)
        for f in storyboardc_dirs
    ])

    apple_support.run(
        actions = actions,
        arguments = args,
        apple_fragment = platform_prerequisites.apple_fragment,
        executable = xctoolrunner,
        execution_requirements = {"no-sandbox": "1"},
        inputs = storyboardc_dirs,
        mnemonic = "StoryboardLink",
        outputs = [output_dir],
        xcode_config = platform_prerequisites.xcode_version_config,
    )

def compile_xib(
        *,
        actions,
        input_file,
        output_dir,
        platform_prerequisites,
        swift_module,
        xctoolrunner):
    """Creates an action that compiles a Xib file.

    Args:
      actions: The actions provider from `ctx.actions`.
      input_file: The Xib file to compile.
      output_dir: The file reference for the output directory.
      platform_prerequisites: Struct containing information on the platform being targeted.
      swift_module: The name of the Swift module to use when compiling the
        Xib file.
      xctoolrunner: A files_to_run for the wrapper around the "xcrun" tool.
    """

    min_os = platform_prerequisites.minimum_os
    families = platform_prerequisites.device_families

    nib_name = paths.replace_extension(paths.basename(input_file.short_path), ".nib")

    args = [
        "ibtool",
        "--compile",
        xctoolrunner_support.prefixed_path(paths.join(output_dir.path, nib_name)),
    ]
    args.extend(_ibtool_arguments(min_os, families))
    args.extend([
        "--module",
        swift_module,
        xctoolrunner_support.prefixed_path(input_file.path),
    ])

    apple_support.run(
        actions = actions,
        arguments = args,
        apple_fragment = platform_prerequisites.apple_fragment,
        executable = xctoolrunner,
        execution_requirements = {"no-sandbox": "1"},
        inputs = [input_file],
        mnemonic = "XibCompile",
        outputs = [output_dir],
        xcode_config = platform_prerequisites.xcode_version_config,
    )
