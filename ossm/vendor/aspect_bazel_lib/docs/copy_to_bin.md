<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A rule that copies source files to the output tree.

This rule uses a Bash command (diff) on Linux/macOS/non-Windows, and a cmd.exe
command (fc.exe) on Windows (no Bash is required).

Originally authored in rules_nodejs
https://github.com/bazel-contrib/rules_nodejs/blob/8b5d27400db51e7027fe95ae413eeabea4856f8e/internal/common/copy_to_bin.bzl

<a id="copy_file_to_bin_action"></a>

## copy_file_to_bin_action

<pre>
copy_file_to_bin_action(<a href="#copy_file_to_bin_action-ctx">ctx</a>, <a href="#copy_file_to_bin_action-file">file</a>)
</pre>

Factory function that creates an action to copy a file to the output tree.

File are copied to the same workspace-relative path. The resulting files is
returned.

If the file passed in is already in the output tree is then it is returned
without a copy action.

To use `copy_file_to_bin_action` in your own rules, you need to include the toolchains it uses
in your rule definition. For example:

```starlark
load("@aspect_bazel_lib//lib:copy_to_bin.bzl", "COPY_FILE_TO_BIN_TOOLCHAINS")

my_rule = rule(
    ...,
    toolchains = COPY_FILE_TO_BIN_TOOLCHAINS,
)
```

Additionally, you must ensure that the coreutils toolchain is has been registered in your
WORKSPACE if you are not using bzlmod:

```starlark
load("@aspect_bazel_lib//lib:repositories.bzl", "register_coreutils_toolchains")

register_coreutils_toolchains()
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_file_to_bin_action-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_file_to_bin_action-file"></a>file |  The file to copy.   |  none |

**RETURNS**

A File in the output tree.


<a id="copy_files_to_bin_actions"></a>

## copy_files_to_bin_actions

<pre>
copy_files_to_bin_actions(<a href="#copy_files_to_bin_actions-ctx">ctx</a>, <a href="#copy_files_to_bin_actions-files">files</a>)
</pre>

Factory function that creates actions to copy files to the output tree.

Files are copied to the same workspace-relative path. The resulting list of
files is returned.

If a file passed in is already in the output tree is then it is added
directly to the result without a copy action.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_files_to_bin_actions-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_files_to_bin_actions-files"></a>files |  List of File objects.   |  none |

**RETURNS**

List of File objects in the output tree.


<a id="copy_to_bin"></a>

## copy_to_bin

<pre>
copy_to_bin(<a href="#copy_to_bin-name">name</a>, <a href="#copy_to_bin-srcs">srcs</a>, <a href="#copy_to_bin-kwargs">kwargs</a>)
</pre>

Copies a source file to output tree at the same workspace-relative path.

e.g. `<execroot>/path/to/file -> <execroot>/bazel-out/<platform>/bin/path/to/file`

If a file passed in is already in the output tree is then it is added directly to the
DefaultInfo provided by the rule without a copy.

This is useful to populate the output folder with all files needed at runtime, even
those which aren't outputs of a Bazel rule.

This way you can run a binary in the output folder (execroot or runfiles_root)
without that program needing to rely on a runfiles helper library or be aware that
files are divided between the source tree and the output tree.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_to_bin-name"></a>name |  Name of the rule.   |  none |
| <a id="copy_to_bin-srcs"></a>srcs |  A list of labels. File(s) to copy.   |  none |
| <a id="copy_to_bin-kwargs"></a>kwargs |  further keyword arguments, e.g. `visibility`   |  none |


