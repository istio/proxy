<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Helpers for making test assertions

<a id="assert_archive_contains"></a>

## assert_archive_contains

<pre>
assert_archive_contains(<a href="#assert_archive_contains-name">name</a>, <a href="#assert_archive_contains-archive">archive</a>, <a href="#assert_archive_contains-expected">expected</a>, <a href="#assert_archive_contains-type">type</a>, <a href="#assert_archive_contains-kwargs">kwargs</a>)
</pre>

Assert that an archive file contains at least the given file entries.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="assert_archive_contains-name"></a>name |  name of the resulting sh_test target   |  none |
| <a id="assert_archive_contains-archive"></a>archive |  Label of the the .tar or .zip file   |  none |
| <a id="assert_archive_contains-expected"></a>expected |  a (partial) file listing, either as a Label of a file containing it, or a list of strings   |  none |
| <a id="assert_archive_contains-type"></a>type |  "tar" or "zip". If None, a type will be inferred from the filename.   |  `None` |
| <a id="assert_archive_contains-kwargs"></a>kwargs |  additional named arguments for the resulting sh_test   |  none |


<a id="assert_contains"></a>

## assert_contains

<pre>
assert_contains(<a href="#assert_contains-name">name</a>, <a href="#assert_contains-actual">actual</a>, <a href="#assert_contains-expected">expected</a>, <a href="#assert_contains-size">size</a>, <a href="#assert_contains-kwargs">kwargs</a>)
</pre>

Generates a test target which fails if the file doesn't contain the string.

Depends on bash, as it creates an sh_test target.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="assert_contains-name"></a>name |  target to create   |  none |
| <a id="assert_contains-actual"></a>actual |  Label of a file   |  none |
| <a id="assert_contains-expected"></a>expected |  a string which should appear in the file   |  none |
| <a id="assert_contains-size"></a>size |  standard attribute for tests   |  `"small"` |
| <a id="assert_contains-kwargs"></a>kwargs |  additional named arguments for the resulting sh_test   |  none |


<a id="assert_directory_contains"></a>

## assert_directory_contains

<pre>
assert_directory_contains(<a href="#assert_directory_contains-name">name</a>, <a href="#assert_directory_contains-directory">directory</a>, <a href="#assert_directory_contains-expected">expected</a>, <a href="#assert_directory_contains-kwargs">kwargs</a>)
</pre>

Assert that a directory contains at least the given file entries.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="assert_directory_contains-name"></a>name |  name of the resulting sh_test target   |  none |
| <a id="assert_directory_contains-directory"></a>directory |  Label of the directory artifact   |  none |
| <a id="assert_directory_contains-expected"></a>expected |  a (partial) file listing, either as a Label of a file containing it, or a list of strings   |  none |
| <a id="assert_directory_contains-kwargs"></a>kwargs |  additional named arguments for the resulting sh_test   |  none |


<a id="assert_json_matches"></a>

## assert_json_matches

<pre>
assert_json_matches(<a href="#assert_json_matches-name">name</a>, <a href="#assert_json_matches-file1">file1</a>, <a href="#assert_json_matches-file2">file2</a>, <a href="#assert_json_matches-filter1">filter1</a>, <a href="#assert_json_matches-filter2">filter2</a>, <a href="#assert_json_matches-kwargs">kwargs</a>)
</pre>

Assert that the given json files have the same semantic content.

Uses jq to filter each file. The default value of `"."` as the filter
means to compare the whole file.

See the [jq rule](./jq.md#jq) for more about the filter expressions as well as
setup notes for the `jq` toolchain.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="assert_json_matches-name"></a>name |  name of resulting diff_test target   |  none |
| <a id="assert_json_matches-file1"></a>file1 |  a json file   |  none |
| <a id="assert_json_matches-file2"></a>file2 |  another json file   |  none |
| <a id="assert_json_matches-filter1"></a>filter1 |  a jq filter to apply to file1   |  `"."` |
| <a id="assert_json_matches-filter2"></a>filter2 |  a jq filter to apply to file2   |  `"."` |
| <a id="assert_json_matches-kwargs"></a>kwargs |  additional named arguments for the resulting diff_test   |  none |


<a id="assert_outputs"></a>

## assert_outputs

<pre>
assert_outputs(<a href="#assert_outputs-name">name</a>, <a href="#assert_outputs-actual">actual</a>, <a href="#assert_outputs-expected">expected</a>, <a href="#assert_outputs-kwargs">kwargs</a>)
</pre>

Assert that the default outputs of a target are the expected ones.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="assert_outputs-name"></a>name |  name of the resulting diff_test   |  none |
| <a id="assert_outputs-actual"></a>actual |  string of the label to check the outputs   |  none |
| <a id="assert_outputs-expected"></a>expected |  a list of rootpaths of expected outputs, as they would appear in a runfiles manifest   |  none |
| <a id="assert_outputs-kwargs"></a>kwargs |  additional named arguments for the resulting diff_test   |  none |


