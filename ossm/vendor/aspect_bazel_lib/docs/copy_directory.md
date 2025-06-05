<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A rule that copies a directory to another place.

The rule uses a precompiled binary to perform the copy, so no shell is required.

## Preserving modification times

`copy_directory` and `copy_to_directory` have a `preserve_mtime` attribute, however
there are two caveats to consider when using this feature:

1. Remote Execution / Caching: These layers will reset the modify time and are
    incompatible with this feature. To avoid these failures the [no-remote tag](https://bazel.build/reference/be/common-definitions)
    can be added.
2. Caching: Changes to only the modified time will not re-trigger cached actions. This can
    be worked around by using a clean build when these types of changes occur. For tests the
    [external tag](https://bazel.build/reference/be/common-definitions) can be used but this
    will result in tests never being cached.

<a id="copy_directory"></a>

## copy_directory

<pre>
copy_directory(<a href="#copy_directory-name">name</a>, <a href="#copy_directory-src">src</a>, <a href="#copy_directory-out">out</a>, <a href="#copy_directory-hardlink">hardlink</a>, <a href="#copy_directory-kwargs">kwargs</a>)
</pre>

Copies a directory to another location.

This rule uses a precompiled binary to perform the copy, so no shell is required.

If using this rule with source directories, it is recommended that you use the
`--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
to files within source directories are detected. See
https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
for more context.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_directory-name"></a>name |  Name of the rule.   |  none |
| <a id="copy_directory-src"></a>src |  The directory to make a copy of. Can be a source directory or TreeArtifact.   |  none |
| <a id="copy_directory-out"></a>out |  Path of the output directory, relative to this package.   |  none |
| <a id="copy_directory-hardlink"></a>hardlink |  Controls when to use hardlinks to files instead of making copies.<br><br>Creating hardlinks is much faster than making copies of files with the caveat that hardlinks share file permissions with their source.<br><br>Since Bazel removes write permissions on files in the output tree after an action completes, hardlinks to source files within source directories is not recommended since write permissions will be inadvertently removed from sources files.<br><br>- "auto": hardlinks are used if src is a tree artifact already in the output tree - "off": files are always copied - "on": hardlinks are always used (not recommended)   |  `"auto"` |
| <a id="copy_directory-kwargs"></a>kwargs |  further keyword arguments, e.g. `visibility`   |  none |


<a id="copy_directory_bin_action"></a>

## copy_directory_bin_action

<pre>
copy_directory_bin_action(<a href="#copy_directory_bin_action-ctx">ctx</a>, <a href="#copy_directory_bin_action-src">src</a>, <a href="#copy_directory_bin_action-dst">dst</a>, <a href="#copy_directory_bin_action-copy_directory_bin">copy_directory_bin</a>, <a href="#copy_directory_bin_action-copy_directory_toolchain">copy_directory_toolchain</a>, <a href="#copy_directory_bin_action-hardlink">hardlink</a>,
                          <a href="#copy_directory_bin_action-verbose">verbose</a>, <a href="#copy_directory_bin_action-preserve_mtime">preserve_mtime</a>)
</pre>

Factory function that creates an action to copy a directory from src to dst using a tool binary.

The tool binary will typically be the `@aspect_bazel_lib//tools/copy_directory` `go_binary`
either built from source or provided by a toolchain.

This helper is used by the copy_directory rule. It is exposed as a public API so it can be used
within other rule implementations.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="copy_directory_bin_action-ctx"></a>ctx |  The rule context.   |  none |
| <a id="copy_directory_bin_action-src"></a>src |  The source directory to copy.   |  none |
| <a id="copy_directory_bin_action-dst"></a>dst |  The directory to copy to. Must be a TreeArtifact.   |  none |
| <a id="copy_directory_bin_action-copy_directory_bin"></a>copy_directory_bin |  Copy to directory tool binary.   |  none |
| <a id="copy_directory_bin_action-copy_directory_toolchain"></a>copy_directory_toolchain |  The toolchain type for Auto Exec Groups. The default is probably what you want.   |  `"@aspect_bazel_lib//lib:copy_directory_toolchain_type"` |
| <a id="copy_directory_bin_action-hardlink"></a>hardlink |  Controls when to use hardlinks to files instead of making copies.<br><br>See copy_directory rule documentation for more details.   |  `"auto"` |
| <a id="copy_directory_bin_action-verbose"></a>verbose |  print verbose logs to stdout   |  `False` |
| <a id="copy_directory_bin_action-preserve_mtime"></a>preserve_mtime |  preserve the modified time from the source. See the caveats above about interactions with remote execution and caching.   |  `False` |


