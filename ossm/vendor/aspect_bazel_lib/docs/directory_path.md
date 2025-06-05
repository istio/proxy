<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rule and corresponding provider that joins a label pointing to a TreeArtifact
with a path nested within that directory

<a id="directory_path"></a>

## directory_path

<pre>
directory_path(<a href="#directory_path-name">name</a>, <a href="#directory_path-directory">directory</a>, <a href="#directory_path-path">path</a>)
</pre>

Provide DirectoryPathInfo to reference some path within a directory.

Otherwise there is no way to give a Bazel label for it.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="directory_path-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="directory_path-directory"></a>directory |  a TreeArtifact (ctx.actions.declare_directory)   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="directory_path-path"></a>path |  path relative to the directory   | String | required |  |


<a id="DirectoryPathInfo"></a>

## DirectoryPathInfo

<pre>
DirectoryPathInfo(<a href="#DirectoryPathInfo-directory">directory</a>, <a href="#DirectoryPathInfo-path">path</a>)
</pre>

Joins a label pointing to a TreeArtifact with a path nested within that directory.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="DirectoryPathInfo-directory"></a>directory |  a TreeArtifact (ctx.actions.declare_directory)    |
| <a id="DirectoryPathInfo-path"></a>path |  path relative to the directory    |


<a id="make_directory_path"></a>

## make_directory_path

<pre>
make_directory_path(<a href="#make_directory_path-name">name</a>, <a href="#make_directory_path-directory">directory</a>, <a href="#make_directory_path-path">path</a>, <a href="#make_directory_path-kwargs">kwargs</a>)
</pre>

Helper function to generate a directory_path target and return its label.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="make_directory_path-name"></a>name |  unique name for the generated `directory_path` target   |  none |
| <a id="make_directory_path-directory"></a>directory |  `directory` attribute passed to generated `directory_path` target   |  none |
| <a id="make_directory_path-path"></a>path |  `path` attribute passed to generated `directory_path` target   |  none |
| <a id="make_directory_path-kwargs"></a>kwargs |  parameters to pass to generated `output_files` target   |  none |

**RETURNS**

The label `name`


<a id="make_directory_paths"></a>

## make_directory_paths

<pre>
make_directory_paths(<a href="#make_directory_paths-name">name</a>, <a href="#make_directory_paths-dict">dict</a>, <a href="#make_directory_paths-kwargs">kwargs</a>)
</pre>

Helper function to convert a dict of directory to path mappings to directory_path targets and labels.

For example,

```
make_directory_paths("my_name", {
    "//directory/artifact:target_1": "file/path",
    "//directory/artifact:target_2": ["file/path1", "file/path2"],
})
```

generates the targets,

```
directory_path(
    name = "my_name_0",
    directory = "//directory/artifact:target_1",
    path = "file/path"
)

directory_path(
    name = "my_name_1",
    directory = "//directory/artifact:target_2",
    path = "file/path1"
)

directory_path(
    name = "my_name_2",
    directory = "//directory/artifact:target_2",
    path = "file/path2"
)
```

and the list of targets is returned,

```
[
    "my_name_0",
    "my_name_1",
    "my_name_2",
]
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="make_directory_paths-name"></a>name |  The target name to use for the generated targets & labels.<br><br>The names are generated as zero-indexed `name + "_" + i`   |  none |
| <a id="make_directory_paths-dict"></a>dict |  The dictionary of directory keys to path or path list values.   |  none |
| <a id="make_directory_paths-kwargs"></a>kwargs |  additional parameters to pass to each generated target   |  none |

**RETURNS**

The label of the generated `directory_path` targets named `name + "_" + i`


