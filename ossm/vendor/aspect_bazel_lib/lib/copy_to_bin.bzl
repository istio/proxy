"""A rule that copies source files to the output tree.

This rule uses a Bash command (diff) on Linux/macOS/non-Windows, and a cmd.exe
command (fc.exe) on Windows (no Bash is required).

Originally authored in rules_nodejs
https://github.com/bazel-contrib/rules_nodejs/blob/8b5d27400db51e7027fe95ae413eeabea4856f8e/internal/common/copy_to_bin.bzl
"""

load(
    "//lib/private:copy_to_bin.bzl",
    _COPY_FILE_TO_BIN_TOOLCHAINS = "COPY_FILE_TO_BIN_TOOLCHAINS",
    _copy_file_to_bin_action = "copy_file_to_bin_action",
    _copy_files_to_bin_actions = "copy_files_to_bin_actions",
    _copy_to_bin = "copy_to_bin",
)

copy_file_to_bin_action = _copy_file_to_bin_action
copy_files_to_bin_actions = _copy_files_to_bin_actions
copy_to_bin = _copy_to_bin
COPY_FILE_TO_BIN_TOOLCHAINS = _COPY_FILE_TO_BIN_TOOLCHAINS
