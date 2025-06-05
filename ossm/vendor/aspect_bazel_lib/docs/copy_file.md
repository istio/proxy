<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A rule that copies a file to another place.

`native.genrule()` is sometimes used to copy files (often wishing to rename them).
The `copy_file` rule does this with a simpler interface than genrule.

This rule uses a hermetic uutils/coreutils `cp` binary, no shell is required.

This fork of bazel-skylib's copy_file adds `DirectoryPathInfo` support and allows multiple
`copy_file` rules in the same package.

<a id="copy_file"></a>

## copy_file

<pre>
copy_file(<a href="#copy_file-name">name</a>, <a href="#copy_file-src">src</a>, <a href="#copy_file-out">out</a>, <a href="#copy_file-is_executable">is_executable</a>, <a href="#copy_file-allow_symlink">allow_symlink</a>, <a href="#copy_file-kwargs">kwargs</a>)
</pre>

Copies a file or directory to another location.

`native.genrule()` is sometimes used to copy files (often wishing to rename them). The 'copy_file' rule does this with a simpler interface than genrule.

This rule uses a hermetic uutils/coreutils `cp` binary, no shell is required.

If using this rule with source directories, it is recommended that you use the
`--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
to files within source directories are detected. See
https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
for more context.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_file-name"></a>name |  Name of the rule.   |  none |
| <a id="copy_file-src"></a>src |  A Label. The file to make a copy of. (Can also be the label of a rule that generates a file.)   |  none |
| <a id="copy_file-out"></a>out |  Path of the output file, relative to this package.   |  none |
| <a id="copy_file-is_executable"></a>is_executable |  A boolean. Whether to make the output file executable. When True, the rule's output can be executed using `bazel run` and can be in the srcs of binary and test rules that require executable sources. WARNING: If `allow_symlink` is True, `src` must also be executable.   |  `False` |
| <a id="copy_file-allow_symlink"></a>allow_symlink |  A boolean. Whether to allow symlinking instead of copying. When False, the output is always a hard copy. When True, the output *can* be a symlink, but there is no guarantee that a symlink is created (i.e., at the time of writing, we don't create symlinks on Windows). Set this to True if you need fast copying and your tools can handle symlinks (which most UNIX tools can).   |  `False` |
| <a id="copy_file-kwargs"></a>kwargs |  further keyword arguments, e.g. `visibility`   |  none |


<a id="copy_file_action"></a>

## copy_file_action

<pre>
copy_file_action(<a href="#copy_file_action-ctx">ctx</a>, <a href="#copy_file_action-src">src</a>, <a href="#copy_file_action-dst">dst</a>, <a href="#copy_file_action-dir_path">dir_path</a>)
</pre>

Factory function that creates an action to copy a file from src to dst.

If src is a TreeArtifact, dir_path must be specified as the path within
the TreeArtifact to the file to copy.

This helper is used by copy_file. It is exposed as a public API so it can be used within
other rule implementations.

To use `copy_file_action` in your own rules, you need to include the toolchains it uses
in your rule definition. For example:

```starlark
load("@aspect_bazel_lib//lib:copy_file.bzl", "COPY_FILE_TOOLCHAINS")

my_rule = rule(
    ...,
    toolchains = COPY_FILE_TOOLCHAINS,
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
| <a id="copy_file_action-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_file_action-src"></a>src |  The source file to copy or TreeArtifact to copy a single file out of.   |  none |
| <a id="copy_file_action-dst"></a>dst |  The destination file.   |  none |
| <a id="copy_file_action-dir_path"></a>dir_path |  If src is a TreeArtifact, the path within the TreeArtifact to the file to copy.   |  `None` |


