# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Utilities and helper rules for Java fuzz tests."""

load("@rules_java//java/common:java_info.bzl", "JavaInfo")
load("//fuzzing/private:binary.bzl", "fuzzing_binary_transition")
load("//fuzzing/private:util.bzl", "runfile_path")

# A Starlark reimplementation of a part of Bazel's JavaCommon#determinePrimaryClass.
def determine_primary_class(srcs, name):
    main_source_path = _get_java_main_source_path(srcs, name)
    return _get_java_full_classname(main_source_path)

# A Starlark reimplementation of a part of Bazel's JavaCommon#determinePrimaryClass.
def _get_java_main_source_path(srcs, name):
    main_source_basename = name + ".java"
    for source_file in srcs:
        if source_file[source_file.rfind("/") + 1:] == main_source_basename:
            main_source_basename = source_file
            break
    return native.package_name() + "/" + main_source_basename[:-len(".java")]

# A Starlark reimplementation of Bazel's JavaUtil#getJavaFullClassname.
def _get_java_full_classname(main_source_path):
    java_path = _get_java_path(main_source_path)
    if java_path != None:
        return java_path.replace("/", ".")
    return None

# A Starlark reimplementation of Bazel's JavaUtil#getJavaPath.
def _get_java_path(main_source_path):
    path_segments = main_source_path.split("/")
    index = _java_segment_index(path_segments)
    if index >= 0:
        return "/".join(path_segments[index + 1:])
    return None

_KNOWN_SOURCE_ROOTS = ["java", "javatests", "src", "testsrc"]

# A Starlark reimplementation of Bazel's JavaUtil#javaSegmentIndex.
def _java_segment_index(path_segments):
    root_index = -1
    for pos, segment in enumerate(path_segments):
        if segment in _KNOWN_SOURCE_ROOTS:
            root_index = pos
            break
    if root_index == -1:
        return root_index

    is_src = "src" == path_segments[root_index]
    check_maven_index = root_index if is_src else -1
    max = len(path_segments) - 1
    if root_index == 0 or is_src:
        for i in range(root_index + 1, max):
            segment = path_segments[i]
            if "src" == segment or (is_src and ("javatests" == segment or "java" == segment)):
                next = path_segments[i + 1]
                if ("com" == next or "org" == next or "net" == next):
                    root_index = i
                elif "src" == segment:
                    check_maven_index = i
                break

    if check_maven_index >= 0 and check_maven_index + 2 < len(path_segments):
        next = path_segments[check_maven_index + 1]
        if "main" == next or "test" == next:
            next = path_segments[check_maven_index + 2]
            if "java" == next or "resources" == next:
                root_index = check_maven_index + 2

    return root_index

def _jazzer_fuzz_binary_script(ctx, target, sanitizer_flags):
    script = ctx.actions.declare_file(ctx.label.name)

    # The script is split into two parts: The first is emitted as-is, the second
    # is a template that is passed to format(). Without the split, curly braces
    # in the first part would need to be escaped.
    script_literal_part = """#!/bin/bash
# LLVMFuzzerTestOneInput - OSS-Fuzz needs this string literal to appear
# somewhere in the script so it is recognized as a fuzz target.

# Bazel-provided code snippet that should be copy-pasted as is at use sites.
# Taken from @bazel_tools//tools/bash/runfiles.
# --- begin runfiles.bash initialization v3 ---
# Copy-pasted from the Bazel Bash runfiles library v3.
set -uo pipefail; set +e; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v3 ---

# Export the env variables required for subprocesses to find their runfiles.
runfiles_export_envvars

# When the runfiles tree exists but does not contain local_jdk, this script is
# executing on OSS-Fuzz. Link the current JAVA_HOME into the runfiles tree.
if [ -d "$0.runfiles" ] && [ ! -d "$0.runfiles/local_jdk" ]; then
    ln -s "$JAVA_HOME" "$0.runfiles/local_jdk"
fi
"""

    script_format_part = """
source "$(rlocation {sanitizer_options})"
if [[ ! -z "{sanitizer_runtime}" ]]; then
  export JAZZER_NATIVE_SANITIZERS_DIR=$(dirname "$(rlocation "{sanitizer_runtime}")")
fi
exec "$(rlocation {target})" {sanitizer_flags} "$@"
"""

    script_content = script_literal_part + script_format_part.format(
        target = runfile_path(ctx, target),
        sanitizer_flags = " ".join(sanitizer_flags),
        sanitizer_options = runfile_path(ctx, ctx.file.sanitizer_options),
        sanitizer_runtime = runfile_path(ctx, ctx.file.sanitizer_runtime) if ctx.file.sanitizer_runtime else "",
    )
    ctx.actions.write(script, script_content, is_executable = True)
    return script

def _jazzer_fuzz_binary_impl(ctx):
    sanitizer = ctx.attr.sanitizer
    sanitizer_flags = []
    if sanitizer in ["asan", "ubsan"]:
        sanitizer_flags.append("--" + sanitizer)
    if not sanitizer_flags and ctx.attr.target[0][JavaInfo].transitive_native_libraries:
        sanitizer_flags.append("--native")

    # Used by the wrapper script created in _jazzer_fuzz_binary_script.
    transitive_runfiles = [
        ctx.attr.target[0][DefaultInfo].default_runfiles,
        ctx.attr._bash_runfiles_library[DefaultInfo].default_runfiles,
        ctx.runfiles(
            [
                ctx.file.sanitizer_options,
            ] + ([ctx.file.sanitizer_runtime] if ctx.file.sanitizer_runtime else []),
        ),
    ]
    runfiles = ctx.runfiles().merge_all(transitive_runfiles)

    target = ctx.attr.target[0][DefaultInfo].files_to_run.executable
    script = _jazzer_fuzz_binary_script(ctx, target, sanitizer_flags)
    return [DefaultInfo(executable = script, runfiles = runfiles)]

jazzer_fuzz_binary = rule(
    implementation = _jazzer_fuzz_binary_impl,
    doc = """
Rule that creates a binary that invokes Jazzer on the specified target.
""",
    attrs = {
        "sanitizer": attr.string(
            values = ["asan", "ubsan", "none"],
        ),
        "sanitizer_options": attr.label(
            doc = "A shell script that can export environment variables with " +
                  "sanitizer options.",
            allow_single_file = [".sh"],
        ),
        "sanitizer_runtime": attr.label(
            doc = "The sanitizer runtime to preload.",
            allow_single_file = [".dylib", ".so"],
        ),
        "target": attr.label(
            doc = "The fuzz target.",
            mandatory = True,
            providers = [JavaInfo],
            cfg = fuzzing_binary_transition,
        ),
        "use_oss_fuzz": attr.bool(),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_bash_runfiles_library": attr.label(
            default = "@bazel_tools//tools/bash/runfiles",
        ),
    },
    executable = True,
)
