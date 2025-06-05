<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A test rule that compares two binary files or two directories.

Similar to `bazel-skylib`'s [`diff_test`](https://github.com/bazelbuild/bazel-skylib/blob/main/rules/diff_test.bzl)
but also supports comparing directories.

The rule uses a Bash command (diff) on Linux/macOS/non-Windows, and a cmd.exe
command (fc.exe) on Windows (no Bash is required).

See also: [rules_diff](https://gitlab.arm.com/bazel/rules_diff)

<a id="diff_test"></a>

## diff_test

<pre>
diff_test(<a href="#diff_test-name">name</a>, <a href="#diff_test-file1">file1</a>, <a href="#diff_test-file2">file2</a>, <a href="#diff_test-diff_args">diff_args</a>, <a href="#diff_test-size">size</a>, <a href="#diff_test-kwargs">kwargs</a>)
</pre>

A test that compares two files.

The test succeeds if the files' contents match.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="diff_test-name"></a>name |  The name of the test rule.   |  none |
| <a id="diff_test-file1"></a>file1 |  Label of the file to compare to <code>file2</code>.   |  none |
| <a id="diff_test-file2"></a>file2 |  Label of the file to compare to <code>file1</code>.   |  none |
| <a id="diff_test-diff_args"></a>diff_args |  Arguments to pass to the `diff` command. (Ignored on Windows)   |  `[]` |
| <a id="diff_test-size"></a>size |  standard attribute for tests   |  `"small"` |
| <a id="diff_test-kwargs"></a>kwargs |  The <a href="https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes-tests">common attributes for tests</a>.   |  none |


