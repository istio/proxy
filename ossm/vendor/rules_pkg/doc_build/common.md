<a name="common"></a>

### Common Attributes

These attributes are used in several rules within this module.

**ATTRIBUTES**

| Name              | Description                                                                                                                                                                     | Type                                                               | Mandatory       | Default                                   |
| :-------------    | :-------------                                                                                                                                                                  | :-------------:                                                    | :-------------: | :-------------                            |
| <a name="out">out</a>               | Name of the output file. This file will always be created and used to access the package content. If `package_file_name` is also specified, `out` will be a symlink.            | String                                                             | required        |                                           |
| <a name="package_file_name">package_file_name</a> | The name of the file which will contain the package. The name may contain variables in the forms `{var}` and $(var)`. The values for substitution are specified through `package_variables` or taken from [ctx.var](https://bazel.build/rules/lib/ctx#var). | String | optional | package type specific |
| <a name="package_variables">package_variables</a> | A target that provides `PackageVariablesInfo` to substitute into `package_file_name`. `pkg_zip` and `pkg_tar` also support this in `package_dir`                                | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional        | None                                      |
| attributes        | Attributes to set on entities created within packages.  Not to be confused with bazel rule attributes.  See 'Mapping "Attributes"' below                                        | Undefined.                                                         | optional        | Varies.  Consult individual rule documentation for details. |

See
[examples/naming_package_files](https://github.com/bazelbuild/rules_pkg/tree/main/examples/naming_package_files)
for examples of how `out`, `package_file_name`, and `package_variables`
interact.

@since(0.8.0): File name substitution now supports the $(var) syntax.
@since(0.8.0): File name substitution now supports direct use of [ctx.var](https://bazel.build/rules/lib/ctx#var).


<a name="mapping-attrs"></a>
### Mapping "Attributes"

The "attributes" attribute specifies properties of package contents as used in
rules such as `pkg_files`, and `pkg_mkdirs`.  These allow fine-grained control
of the contents of your package.  For example:

```python
attributes = pkg_attributes(
    mode = "0644",
    user = "root",
    group = "wheel",
    my_custom_attribute = "some custom value",
)
```

`mode`, `user`, and `group` correspond to common UNIX-style filesystem
permissions.  Attributes should always be specified using the `pkg_attributes`
helper macro.

Each mapping rule has some default mapping attributes.  At this time, the only
default is "mode", which will be set if it is not otherwise overridden by the user.

If `user` and `group` are not specified, then defaults for them will be chosen
by the underlying package builder.  Any specific behavior from package builders
should not be relied upon.

Any other attributes should be specified as additional arguments to
`pkg_attributes`.
