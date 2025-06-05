# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Rule for packaging fuzz tests in the expected OSS-Fuzz format."""

load("//fuzzing/private:binary.bzl", "FuzzingBinaryInfo")
load("//fuzzing/private:util.bzl", "runfile_path")

def _oss_fuzz_package_impl(ctx):
    output_archive = ctx.actions.declare_file(ctx.label.name + ".tar")
    binary_info = ctx.attr.binary[FuzzingBinaryInfo]

    binary_runfiles = binary_info.binary_runfiles.files.to_list()
    archive_inputs = binary_runfiles

    runfiles_manifest = ctx.actions.declare_file(ctx.label.name + "_runfiles")
    local_jdk_prefix = Label("@local_jdk//:unused").workspace_name + "/"
    runfiles_manifest_content = "".join([
        "{runfile_path} {real_path}\n".format(
            real_path = runfile.path,
            runfile_path = runfile_path(ctx, runfile),
        )
        # In order not to duplicate the fuzz test binary, it is excluded from
        # the runfiles here. This deviates from the usual Bazel runfiles layout,
        # but is required since ClusterFuzz executes fuzz targets in
        # subdirectories and would thus duplicate every C++ fuzz target.
        # We also exclude the local JDK as OSS-Fuzz provides one.
        for runfile in binary_runfiles
        if runfile != binary_info.binary_file and not runfile_path(ctx, runfile).startswith(local_jdk_prefix)
    ])
    ctx.actions.write(runfiles_manifest, runfiles_manifest_content, False)
    archive_inputs.append(runfiles_manifest)

    if binary_info.binary_repo_mapping_manifest:
        archive_inputs.append(binary_info.binary_repo_mapping_manifest)
    if binary_info.corpus_dir:
        archive_inputs.append(binary_info.corpus_dir)
    if binary_info.dictionary_file:
        archive_inputs.append(binary_info.dictionary_file)
    ctx.actions.run_shell(
        outputs = [output_archive],
        inputs = archive_inputs,
        command = """
            set -e
            declare -r STAGING_DIR="$(mktemp --directory -t oss-fuzz-pkg.XXXXXXXXXX)"
            function cleanup() {{
                rm -rf "$STAGING_DIR"
            }}
            trap cleanup EXIT
            ln -s "$(pwd)/{binary_path}" "$STAGING_DIR/{base_name}"
            while IFS= read -r line; do
              IFS=' ' read -r link target <<< "$line"
              mkdir -p "$(dirname "$STAGING_DIR/{binary_runfiles_dir}/$link")"
              ln -s "$(pwd)/$target" "$STAGING_DIR/{binary_runfiles_dir}/$link"
            done <{runfiles_manifest_path}
            if [[ -n "{binary_repo_mapping_manifest}" ]]; then
                mkdir -p "$STAGING_DIR/{binary_runfiles_dir}"
                ln -s "$(pwd)/{binary_repo_mapping_manifest}" "$STAGING_DIR/{binary_runfiles_dir}/_repo_mapping"
            fi
            if [[ -n "{corpus_dir}" ]]; then
                pushd "{corpus_dir}" >/dev/null
                zip --quiet -r "$STAGING_DIR/{base_name}_seed_corpus.zip" ./*
                popd >/dev/null
            fi
            if [[ -n "{dictionary_path}" ]]; then
                ln -s "$(pwd)/{dictionary_path}" "$STAGING_DIR/{base_name}.dict"
            fi
            if [[ -n "{options_path}" ]]; then
                ln -s "$(pwd)/{options_path}" "$STAGING_DIR/{base_name}.options"
            fi
            tar -czhf "{output}" -C "$STAGING_DIR" .
        """.format(
            base_name = ctx.attr.base_name,
            binary_path = binary_info.binary_file.path,
            binary_runfiles_dir = ctx.attr.base_name + ".runfiles",
            binary_repo_mapping_manifest = binary_info.binary_repo_mapping_manifest.path if binary_info.binary_repo_mapping_manifest else "",
            corpus_dir = binary_info.corpus_dir.path if binary_info.corpus_dir else "",
            dictionary_path = binary_info.dictionary_file.path if binary_info.dictionary_file else "",
            options_path = binary_info.options_file.path if binary_info.options_file else "",
            output = output_archive.path,
            runfiles_manifest_path = runfiles_manifest.path,
        ),
    )
    return [DefaultInfo(files = depset([output_archive]))]

oss_fuzz_package = rule(
    implementation = _oss_fuzz_package_impl,
    doc = """
Packages a fuzz test in a TAR archive compatible with the OSS-Fuzz format.
""",
    attrs = {
        "binary": attr.label(
            executable = True,
            doc = "The fuzz test executable.",
            providers = [FuzzingBinaryInfo],
            mandatory = True,
            cfg = "target",
        ),
        "base_name": attr.string(
            doc = "The base name of the fuzz test used to form the file names " +
                  "in the OSS-Fuzz output.",
            mandatory = True,
        ),
    },
)
