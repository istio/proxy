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

"Set defaults for the pip-compile command to run it under Bazel"

import atexit
import os
import shutil
import sys
from pathlib import Path
from typing import Optional, Tuple

import click
import piptools.writer as piptools_writer
from piptools.scripts.compile import cli

from python.runfiles import runfiles

# Replace the os.replace function with shutil.copy to work around os.replace not being able to
# replace or move files across filesystems.
os.replace = shutil.copy

# Next, we override the annotation_style_split and annotation_style_line functions to replace the
# backslashes in the paths with forward slashes. This is so that we can have the same requirements
# file on Windows and Unix-like.
original_annotation_style_split = piptools_writer.annotation_style_split
original_annotation_style_line = piptools_writer.annotation_style_line


def annotation_style_split(required_by) -> str:
    required_by = set([v.replace("\\", "/") for v in required_by])
    return original_annotation_style_split(required_by)


def annotation_style_line(required_by) -> str:
    required_by = set([v.replace("\\", "/") for v in required_by])
    return original_annotation_style_line(required_by)


piptools_writer.annotation_style_split = annotation_style_split
piptools_writer.annotation_style_line = annotation_style_line


def _select_golden_requirements_file(
    requirements_txt, requirements_linux, requirements_darwin, requirements_windows
):
    """Switch the golden requirements file, used to validate if updates are needed,
    to a specified platform specific one.  Fallback on the platform independent one.
    """

    plat = sys.platform
    if plat == "linux" and requirements_linux is not None:
        return requirements_linux
    elif plat == "darwin" and requirements_darwin is not None:
        return requirements_darwin
    elif plat == "win32" and requirements_windows is not None:
        return requirements_windows
    else:
        return requirements_txt


def _locate(bazel_runfiles, file):
    """Look up the file via Rlocation"""

    if not file:
        return file

    return bazel_runfiles.Rlocation(file)


@click.command(context_settings={"ignore_unknown_options": True})
@click.option("--src", "srcs", multiple=True, required=True)
@click.argument("requirements_txt")
@click.argument("update_target_label")
@click.option("--requirements-linux")
@click.option("--requirements-darwin")
@click.option("--requirements-windows")
@click.argument("extra_args", nargs=-1, type=click.UNPROCESSED)
def main(
    srcs: Tuple[str, ...],
    requirements_txt: str,
    update_target_label: str,
    requirements_linux: Optional[str],
    requirements_darwin: Optional[str],
    requirements_windows: Optional[str],
    extra_args: Tuple[str, ...],
) -> None:
    bazel_runfiles = runfiles.Create()

    requirements_file = _select_golden_requirements_file(
        requirements_txt=requirements_txt,
        requirements_linux=requirements_linux,
        requirements_darwin=requirements_darwin,
        requirements_windows=requirements_windows,
    )

    resolved_srcs = [_locate(bazel_runfiles, src) for src in srcs]
    resolved_requirements_file = _locate(bazel_runfiles, requirements_file)

    # Files in the runfiles directory has the following naming schema:
    # Main repo: __main__/<path_to_file>
    # External repo: <workspace name>/<path_to_file>
    # We want to strip both __main__ and <workspace name> from the absolute prefix
    # to keep the requirements lock file agnostic.
    repository_prefix = requirements_file[: requirements_file.index("/") + 1]
    absolute_path_prefix = resolved_requirements_file[
        : -(len(requirements_file) - len(repository_prefix))
    ]

    # As srcs might contain references to generated files we want to
    # use the runfiles file first. Thus, we need to compute the relative path
    # from the execution root.
    # Note: Windows cannot reference generated files without runfiles support enabled.
    srcs_relative = [src[len(repository_prefix) :] for src in srcs]
    requirements_file_relative = requirements_file[len(repository_prefix) :]

    # Before loading click, set the locale for its parser.
    # If it leaks through to the system setting, it may fail:
    # RuntimeError: Click will abort further execution because Python 3 was configured to use ASCII
    # as encoding for the environment. Consult https://click.palletsprojects.com/python3/ for
    # mitigation steps.
    os.environ["LC_ALL"] = "C.UTF-8"
    os.environ["LANG"] = "C.UTF-8"

    argv = []

    UPDATE = True
    # Detect if we are running under `bazel test`.
    if "TEST_TMPDIR" in os.environ:
        UPDATE = False
        # pip-compile wants the cache files to be writeable, but if we point
        # to the real user cache, Bazel sandboxing makes the file read-only
        # and we fail.
        # In theory this makes the test more hermetic as well.
        argv.append(f"--cache-dir={os.environ['TEST_TMPDIR']}")
        # Make a copy for pip-compile to read and mutate.
        requirements_out = os.path.join(
            os.environ["TEST_TMPDIR"], os.path.basename(requirements_file) + ".out"
        )
        # Those two files won't necessarily be on the same filesystem, so we can't use os.replace
        # or shutil.copyfile, as they will fail with OSError: [Errno 18] Invalid cross-device link.
        shutil.copy(resolved_requirements_file, requirements_out)

    update_command = os.getenv("CUSTOM_COMPILE_COMMAND") or "bazel run %s" % (
        update_target_label,
    )

    os.environ["CUSTOM_COMPILE_COMMAND"] = update_command
    os.environ["PIP_CONFIG_FILE"] = os.getenv("PIP_CONFIG_FILE") or os.devnull

    argv.append(
        f"--output-file={requirements_file_relative if UPDATE else requirements_out}"
    )
    argv.extend(
        (src_relative if Path(src_relative).exists() else resolved_src)
        for src_relative, resolved_src in zip(srcs_relative, resolved_srcs)
    )
    argv.extend(extra_args)

    if UPDATE:
        print("Updating " + requirements_file_relative)

        # Make sure the output file for pip_compile exists. It won't if we are on Windows and --enable_runfiles is not set.
        if not os.path.exists(requirements_file_relative):
            os.makedirs(os.path.dirname(requirements_file_relative), exist_ok=True)
            shutil.copy(resolved_requirements_file, requirements_file_relative)

        if "BUILD_WORKSPACE_DIRECTORY" in os.environ:
            workspace = os.environ["BUILD_WORKSPACE_DIRECTORY"]
            requirements_file_tree = os.path.join(workspace, requirements_file_relative)
            absolute_output_file = Path(requirements_file_relative).absolute()
            # In most cases, requirements_file will be a symlink to the real file in the source tree.
            # If symlinks are not enabled (e.g. on Windows), then requirements_file will be a copy,
            # and we should copy the updated requirements back to the source tree.
            if not absolute_output_file.samefile(requirements_file_tree):
                atexit.register(
                    lambda: shutil.copy(
                        absolute_output_file, requirements_file_tree
                    )
                )
        cli(argv, standalone_mode = False)
        requirements_file_relative_path = Path(requirements_file_relative)
        content = requirements_file_relative_path.read_text()
        content = content.replace(absolute_path_prefix, "")
        requirements_file_relative_path.write_text(content)
    else:
        # cli will exit(0) on success
        try:
            print("Checking " + requirements_file)
            cli(argv)
            print("cli() should exit", file=sys.stderr)
            sys.exit(1)
        except SystemExit as e:
            if e.code == 2:
                print(
                    "pip-compile exited with code 2. This means that pip-compile found "
                    "incompatible requirements or could not find a version that matches "
                    f"the install requirement in one of {srcs_relative}.",
                    file=sys.stderr,
                )
                sys.exit(1)
            elif e.code == 0:
                golden = open(_locate(bazel_runfiles, requirements_file)).readlines()
                out = open(requirements_out).readlines()
                out = [line.replace(absolute_path_prefix, "") for line in out]
                if golden != out:
                    import difflib

                    print("".join(difflib.unified_diff(golden, out)), file=sys.stderr)
                    print(
                        "Lock file out of date. Run '"
                        + update_command
                        + "' to update.",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                sys.exit(0)
            else:
                print(
                    f"pip-compile unexpectedly exited with code {e.code}.",
                    file=sys.stderr,
                )
                sys.exit(1)


if __name__ == "__main__":
    main()
