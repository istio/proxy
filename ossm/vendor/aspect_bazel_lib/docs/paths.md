<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Utilities for working with file paths.

<a id="relative_file"></a>

## relative_file

<pre>
relative_file(<a href="#relative_file-to_file">to_file</a>, <a href="#relative_file-frm_file">frm_file</a>)
</pre>

Resolves a relative path between two files, "to_file" and "frm_file".

If neither of the paths begin with ../ it is assumed that they share the same root. When finding the relative path,
the incoming files are treated as actual files (not folders) so the resulting relative path may differ when compared
to passing the same arguments to python's "os.path.relpath()" or NodeJs's "path.relative()".

For example, 'relative_file("../foo/foo.txt", "bar/bar.txt")' will return '../../foo/foo.txt'


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="relative_file-to_file"></a>to_file |  the path with file name to resolve to, from frm   |  none |
| <a id="relative_file-frm_file"></a>frm_file |  the path with file name to resolve from   |  none |

**RETURNS**

The relative path from frm_file to to_file, including the file name


<a id="to_output_relative_path"></a>

## to_output_relative_path

<pre>
to_output_relative_path(<a href="#to_output_relative_path-file">file</a>)
</pre>

The relative path from bazel-out/[arch]/bin to the given File object

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="to_output_relative_path-file"></a>file |  a `File` object   |  none |

**RETURNS**

The output relative path for the `File`


<a id="to_repository_relative_path"></a>

## to_repository_relative_path

<pre>
to_repository_relative_path(<a href="#to_repository_relative_path-file">file</a>)
</pre>

The repository relative path for a `File`

This is the full runfiles path of a `File` excluding its workspace name.

This differs from  root path (a.k.a. [short_path](https://bazel.build/rules/lib/File#short_path)) and
rlocation path as it does not include the repository name if the `File` is from an external repository.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="to_repository_relative_path-file"></a>file |  a `File` object   |  none |

**RETURNS**

The repository relative path for the `File`


<a id="to_rlocation_path"></a>

## to_rlocation_path

<pre>
to_rlocation_path(<a href="#to_rlocation_path-ctx">ctx</a>, <a href="#to_rlocation_path-file">file</a>)
</pre>

The rlocation path for a `File`

This produces the same value as the `rlocationpath` predefined source/output path variable.

From https://bazel.build/reference/be/make-variables#predefined_genrule_variables:

> `rlocationpath`: The path a built binary can pass to the `Rlocation` function of a runfiles
> library to find a dependency at runtime, either in the runfiles directory (if available)
> or using the runfiles manifest.

> This is similar to root path (a.k.a. [short_path](https://bazel.build/rules/lib/File#short_path))
> in that it does not contain configuration prefixes, but differs in that it always starts with the
> name of the repository.

> The rlocation path of a `File` in an external repository repo will start with `repo/`, followed by the
> repository-relative path.

> Passing this path to a binary and resolving it to a file system path using the runfiles libraries
> is the preferred approach to find dependencies at runtime. Compared to root path, it has the
> advantage that it works on all platforms and even if the runfiles directory is not available.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="to_rlocation_path-ctx"></a>ctx |  starlark rule execution context   |  none |
| <a id="to_rlocation_path-file"></a>file |  a `File` object   |  none |

**RETURNS**

The rlocationpath for the `File`


