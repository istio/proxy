<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API for expanding variables

<a id="expand_locations"></a>

## expand_locations

<pre>
expand_locations(<a href="#expand_locations-ctx">ctx</a>, <a href="#expand_locations-input">input</a>, <a href="#expand_locations-targets">targets</a>)
</pre>

Expand location templates.

Expands all `$(execpath ...)`, `$(rootpath ...)` and deprecated `$(location ...)` templates in the
given string by replacing with the expanded path. Expansion only works for labels that point to direct dependencies
of this rule or that are explicitly listed in the optional argument targets.

See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.

Use `$(rootpath)` and `$(rootpaths)` to expand labels to the runfiles path that a built binary can use
to find its dependencies. This path is of the format:
- `./file`
- `path/to/file`
- `../external_repo/path/to/file`

Use `$(execpath)` and `$(execpaths)` to expand labels to the execroot (where Bazel runs build actions).
This is of the format:
- `./file`
- `path/to/file`
- `external/external_repo/path/to/file`
- `<bin_dir>/path/to/file`
- `<bin_dir>/external/external_repo/path/to/file`

The deprecated `$(location)` and `$(locations)` expansions returns either the execpath or rootpath depending on the context.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="expand_locations-ctx"></a>ctx |  context   |  none |
| <a id="expand_locations-input"></a>input |  String to be expanded   |  none |
| <a id="expand_locations-targets"></a>targets |  List of targets for additional lookup information.   |  `[]` |

**RETURNS**

The expanded path or the original path

**DEPRECATED**

Use vanilla `ctx.expand_location(input, targets = targets)` instead


<a id="expand_variables"></a>

## expand_variables

<pre>
expand_variables(<a href="#expand_variables-ctx">ctx</a>, <a href="#expand_variables-s">s</a>, <a href="#expand_variables-outs">outs</a>, <a href="#expand_variables-attribute_name">attribute_name</a>)
</pre>

Expand make variables and substitute like genrule does.

Bazel [pre-defined variables](https://bazel.build/reference/be/make-variables#predefined_variables)
are expanded however only `$@`, `$(@D)` and `$(RULEDIR)` of
[pre-defined genrule variables](https://bazel.build/reference/be/make-variables#predefined_genrule_variables)
are supported.

This function is the same as ctx.expand_make_variables with the additional
genrule-like substitutions of:

  - `$@`: The output file if it is a single file. Else triggers a build error.

  - `$(@D)`: The output directory.

    If there is only one file name in outs, this expands to the directory containing that file.

    If there is only one directory in outs, this expands to the single output directory.

    If there are multiple files, this instead expands to the package's root directory in the bin tree,
    even if all generated files belong to the same subdirectory!

  - `$(RULEDIR)`: The output directory of the rule, that is, the directory
    corresponding to the name of the package containing the rule under the bin tree.

  - `$(BUILD_FILE_PATH)`: ctx.build_file_path

  - `$(VERSION_FILE)`: ctx.version_file.path

  - `$(INFO_FILE)`: ctx.info_file.path

  - `$(TARGET)`: ctx.label

  - `$(WORKSPACE)`: ctx.workspace_name

See https://docs.bazel.build/versions/main/be/general.html#genrule.cmd and
https://docs.bazel.build/versions/main/be/make-variables.html#predefined_genrule_variables
for more information of how these special variables are expanded.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="expand_variables-ctx"></a>ctx |  starlark rule context   |  none |
| <a id="expand_variables-s"></a>s |  expression to expand   |  none |
| <a id="expand_variables-outs"></a>outs |  declared outputs of the rule, for expanding references to outputs   |  `[]` |
| <a id="expand_variables-attribute_name"></a>attribute_name |  name of the attribute containing the expression. Used for error reporting.   |  `"args"` |

**RETURNS**

`s` with the variables expanded


