:::{default-domain} bzl
:::
:::{bzl:currentfile} //command_line_option:BUILD.bazel
:::

# //command_line_option

This package provides special targets that correspond to the Bazel-builtin
`//command_line_option` psuedo-targets. These can be used with the {obj}`config_settings`
attribute on Python rules to transition specific command line flags for a target.

:::{note}
These targets are not actual `alias()` targets, Starlark flags, nor are they the
actual builtin command line flags. They are regular targets that the
`config_settings` transition logic specially recognizes and handles as if they
were the builtin `//command_line_option` psuedo-targets.
:::

While this package only provides a subset of builtin Bazel flags, additional
ones can be introduced by:

* Define your own `@foo//command_line_flag:<name>` target. It **must** be
  in a top-level `command_line_flag` directory.
* Use {obj}`config.add_transition_setting` to make the rules transition on
  the corresponding `//command_line_option:<name>` builtin Bazel psuedo-target.

:::{seealso}
The `config_settings` attribute documentation on:
* {obj}`py_binary.config_settings`
* {obj}`py_test.config_settings`
:::

## build_runfile_links

:::{bzl:target} build_runfile_links

Special target for the Bazel-builtin `//command_line_option:build_runfile_links` flag.

See the [Bazel documentation for --build_runfile_links](https://bazel.build/reference/command-line-reference#flag--build_runfile_links).

The special value `INHERIT` can be specified to use the existing flag value.
:::

## enable_runfiles

:::{bzl:target} enable_runfiles

Special target for the Bazel-builtin `//command_line_option:enable_runfiles` flag.

See the [Bazel documentation for --enable_runfiles](https://bazel.build/reference/command-line-reference#flag--enable_runfiles).

The special value `INHERIT` can be specified to use the existing flag value.
:::
