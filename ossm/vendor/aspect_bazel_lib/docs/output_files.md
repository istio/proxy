<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A rule that provides file(s) specific via DefaultInfo from a given target's DefaultInfo or OutputGroupInfo.

See also [select_file](https://github.com/bazelbuild/bazel-skylib/blob/main/docs/select_file_doc.md) from bazel-skylib.

<a id="output_files"></a>

## output_files

<pre>
output_files(<a href="#output_files-name">name</a>, <a href="#output_files-output_group">output_group</a>, <a href="#output_files-paths">paths</a>, <a href="#output_files-target">target</a>)
</pre>

A rule that provides file(s) specific via DefaultInfo from a given target's DefaultInfo or OutputGroupInfo

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="output_files-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="output_files-output_group"></a>output_group |  if set, we look in the specified output group for paths instead of DefaultInfo   | String | optional |  `""`  |
| <a id="output_files-paths"></a>paths |  the paths of the file(s), relative to their roots, to provide via DefaultInfo from the given target's DefaultInfo or OutputGroupInfo   | List of strings | required |  |
| <a id="output_files-target"></a>target |  the target to look in for requested paths in its' DefaultInfo or OutputGroupInfo   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="make_output_files"></a>

## make_output_files

<pre>
make_output_files(<a href="#make_output_files-name">name</a>, <a href="#make_output_files-target">target</a>, <a href="#make_output_files-paths">paths</a>, <a href="#make_output_files-kwargs">kwargs</a>)
</pre>

Helper function to generate a output_files target and return its label.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="make_output_files-name"></a>name |  unique name for the generated `output_files` target   |  none |
| <a id="make_output_files-target"></a>target |  `target` attribute passed to generated `output_files` target   |  none |
| <a id="make_output_files-paths"></a>paths |  `paths` attribute passed to generated `output_files` target   |  none |
| <a id="make_output_files-kwargs"></a>kwargs |  parameters to pass to generated `output_files` target   |  none |

**RETURNS**

The label `name`


