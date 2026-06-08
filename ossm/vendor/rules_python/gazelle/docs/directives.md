# Directives

You can configure the extension using directives, just like for other
languages. These are just comments in the `BUILD.bazel` file which
govern behavior of the extension when processing files under that
folder.

See the [Gazelle docs on directives][gazelle-directives] for some general
directives that may be useful. In particular, the `resolve` directive
is language-specific and can be used with Python. Examples of these and
the Python-specific directives in use can be found in the
{gh-path}`gazelle/testdata` folder in the `rules_python` repo.

[gazelle-directives]: https://github.com/bazelbuild/bazel-gazelle#directives

The Python-specific directives are:

{.glossary}
[`# gazelle:python_extension value`](#directive-python-extension)
: Controls whether the Python extension is enabled or not. Sub-packages
  inherit this value.
  * Default: `enabled`
  * Allowed Values: `enabled`, `disabled`

[`# gazelle:python_root`](#directive-python-root)
: Sets a Bazel package as a Python root. This is used on monorepos with
  multiple Python projects that don't share the top-level of the workspace
  as the root.
  * Default: n/a
  * Allowed Values: None. This direcive does not consume values.

[`# gazelle:python_manifest_file_name value`](#directive-python-manifest-file-name)
: Overrides the default manifest file name.
  * Default: `gazelle_python.yaml`
  * Allowed Values: A string

[`# gazelle:python_ignore_files value`](#directive-python-ignore-files)
: Controls the files which are ignored from the generated targets.
  * Default: n/a
  * Allowed Values: A comma-separated list of strings.

[`# gazelle:python_ignore_dependencies value`](#directive-python-ignore-dependencies)
: Controls the ignored dependencies from the generated targets.
  * Default: n/a
  * Allowed Values: A comma-separated list of strings.

[`# gazelle:python_validate_import_statements bool`](#directive-python-validate-import-statements)
: Controls whether the Python import statements should be validated.
  * Default: `true`
  * Allowed Values: `true`, `false`

[`# gazelle:python_generation_mode value`](#directive-python-generation-mode)
: Controls the target generation mode.
  * Default: `package`
  * Allowed Values: `file`, `package`, `project`

[`# gazelle:python_generation_mode_per_file_include_init bool`](#directive-python-generation-mode-per-file-include-init)
: Controls whether `__init__.py` files are included as srcs in each
  generated target when target generation mode is "file".
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_generation_mode_per_package_require_test_entry_point bool`](#directive-python-generation-mode-per-package-require-test-entry-point)
: Controls whether a file called `__test__.py` or a target called
  `__test__` is required to generate one test target per package in
  package mode.
  * Default: `true`
  * Allowed Values: `true`, `false`

[`# gazelle:python_library_naming_convention value`](#directive-python-library-naming-convention)
: Controls the {bzl:obj}`py_library` naming convention. It interpolates
  `$package_name$` with the Bazel package name. E.g. if the Bazel package
  name is `foo`, setting this to `$package_name$_my_lib` would result in a
  generated target named `foo_my_lib`.
  * Default: `$package_name$`
  * Allowed Values: A string containing `"$package_name$"`

[`# gazelle:python_binary_naming_convention value`](#directive-python-binary-naming-convention)
: Controls the {bzl:obj}`py_binary` naming convention. Follows the same interpolation
  rules as `python_library_naming_convention`.
  * Default: `$package_name$_bin`
  * Allowed Values: A string containing `"$package_name$"`

[`# gazelle:python_test_naming_convention value`](#directive-python-test-naming-convention)
: Controls the {bzl:obj}`py_test` naming convention. Follows the same interpolation
  rules as `python_library_naming_convention`.
  * Default: `$package_name$_test`
  * Allowed Values: A string containing `"$package_name$"`

[`# gazelle:python_proto_naming_convention value`](#directive-python-proto-naming-convention)
: Controls the {bzl:obj}`py_proto_library` naming convention. It interpolates
  `$proto_name$` with the `proto_library` rule name, minus any trailing
  `_proto`. E.g. if the `proto_library` name is `foo_proto`, setting this
  to `$proto_name$_my_lib` would render to `foo_my_lib`.
  * Default: `$proto_name$_py_pb2`
  * Allowed Values: A string containing `"$proto_name$"`

[`# gazelle:resolve py import-lang import-string label`](#directive-resolve-py)
: Instructs the plugin what target to add as a dependency to satisfy a given
  import statement. The syntax is `# gazelle:resolve py import-string label`
  where `import-string` is the symbol in the python `import` statement,
  and `label` is the Bazel label that Gazelle should write in `deps`.
  * Default: n/a
  * Allowed Values: See the [bazel-gazelle docs][gazelle-directives]

[`# gazelle:python_default_visibility labels`](#directive-python-default-visibility)
: Instructs gazelle to use these visibility labels on all python targets.
  `labels` is a comma-separated list of labels (without spaces).
  * Default: `//$python_root$:__subpackages__`
  * Allowed Values: A string

[`# gazelle:python_visibility label`](#directive-python-visibility)
: Appends additional visibility labels to each generated target. This r
  directive can be set multiple times.
  * Default: n/a
  * Allowed Values: A string

[`# gazelle:python_test_file_pattern value`](#directive-python-test-file-pattern)
: Filenames matching these comma-separated {command}`glob`s will be mapped to
  {bzl:obj}`py_test` targets.
  * Default: `*_test.py,test_*.py`
  * Allowed Values: A glob string

[`# gazelle:python_label_convention value`](#directive-python-label-convention)
: Defines the format of the distribution name in labels to third-party deps.
  Useful for using Gazelle plugin with other rules with different repository
  conventions (e.g. `rules_pycross`). Full label is always prepended with
  the `pip` repository name, e.g. `@pip//numpy` if your
  `MODULE.bazel` has `use_repo(pip, "pip")` or `@pypi//numpy`
  if your `MODULE.bazel` has `use_repo(pip, "pypi")`.
  * Default: `$distribution_name$`
  * Allowed Values: A string

[`# gazelle:python_label_normalization value`](#directive-python-label-normalization)
: Controls how distribution names in labels to third-party deps are
  normalized. Useful for using Gazelle plugin with other rules with different
  label conventions (e.g. `rules_pycross` uses PEP-503).
  * Default: `snake_case`
  * Allowed Values: `snake_case`, `none`, `pep503`

[`# gazelle:python_experimental_allow_relative_imports bool`](#directive-python-experimental-allow-relative-imports)
: Controls whether Gazelle resolves dependencies for import statements that
  use paths relative to the current package.
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_generate_pyi_deps bool`](#directive-python-generate-pyi-deps)
: Controls whether to generate a separate `pyi_deps` attribute for
  type-checking dependencies or merge them into the regular `deps`
  attribute. When `false` (default), type-checking dependencies are
  merged into `deps` for backward compatibility. When `true`, generates
  separate `pyi_deps`. Imports in blocks with the format
  `if typing.TYPE_CHECKING:` or `if TYPE_CHECKING:` and type-only stub
  packages (eg. boto3-stubs) are recognized as type-checking dependencies.
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_generate_pyi_srcs bool`](#directive-python-generate-pyi-srcs)
: Controls whether to generate a `pyi_srcs` attribute if a sibling `.pyi` file
  is found. When `false` (default), the `pyi_srcs` attribute is not added.
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_generate_proto bool`](#directive-python-generate-proto)
: Controls whether to generate a {bzl:obj}`py_proto_library` for each
  `proto_library` in the package. By default we load this rule from the
  `@protobuf` repository; use `gazelle:map_kind` if you need to load this
  from somewhere else.
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_resolve_sibling_imports bool`](#directive-python-resolve-sibling-imports)
: Allows absolute imports to be resolved to sibling modules (Python 2's
  behavior without `absolute_import`).
  * Default: `false`
  * Allowed Values: `true`, `false`

[`# gazelle:python_include_ancestor_conftest bool`](#directive-python-include-ancestor-conftest)
: Controls whether ancestor conftest targets are added to {bzl:obj}`py_test` target
  dependencies.
  * Default: `true`
  * Allowed Values: `true`, `false`

(directive-python-extension)=
## `python_extension`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-root)=
## `python_root`

Set this directive within the Bazel package that you want to use as the Python root.
For example, if using a `src` dir (as recommended by the [Python Packaging User
Guide][python-packaging-user-guide]), then set this directive in `src/BUILD.bazel`:

```starlark
# ./src/BUILD.bazel
# Tell gazelle that are python root is the same dir as this Bazel package.
# gazelle:python_root
```

Note that the directive does not have any arguments.

Gazelle will then add the necessary `imports` attribute to all targets that it
generates:

```starlark
# in ./src/foo/BUILD.bazel
py_libary(
    ...
    imports = [".."],  # Gazelle adds this
    ...
)

# in ./src/foo/bar/BUILD.bazel
py_libary(
    ...
    imports = ["../.."],  # Gazelle adds this
    ...
)
```

[python-packaging-user-guide]: https://github.com/pypa/packaging.python.org/blob/4c86169a/source/tutorials/packaging-projects.rst


(directive-python-manifest-file-name)=
## `python_manifest_file_name`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-ignore-files)=
## `python_ignore_files`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-ignore-dependencies)=
## `python_ignore_dependencies`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-validate-import-statements)=
## `python_validate_import_statements`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-generation-mode)=
## `python_generation_mode`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-generation-mode-per-file-include-init)=
## `python_generation_mode_per_file_include_init`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-generation-mode-per-package-require-test-entry-point)=
## `python_generation_mode_per_package_require_test_entry_point`

When `# gazelle:python_generation_mode package`, whether a file called
`__test__.py` or a target called `__test__`, a.k.a., entry point, is required
to generate one test target per package. If this is set to true but no entry
point is found, Gazelle will fall back to file mode and generate one test target
per file. Setting this directive to false forces Gazelle to generate one test
target per package even without entry point. However, this means the `main`
attribute of the {bzl:obj}`py_test` will not be set and the target will not be runnable
unless either:

1.  there happen to be a file in the `srcs` with the same name as the {bzl:obj}`py_test`
    target, or
2.  a macro populating the `main` attribute of {bzl:obj}`py_test` is configured with
    `gazelle:map_kind` to replace {bzl:obj}`py_test` when Gazelle is generating Python
    test targets. For example, user can provide such a macro to Gazelle:

```starlark
load("@rules_python//python:defs.bzl", _py_test="py_test")
load("@aspect_rules_py//py:defs.bzl", "py_pytest_main")

def py_test(name, main=None, **kwargs):
    deps = kwargs.pop("deps", [])
    if not main:
        py_pytest_main(
            name = "__test__",
            deps = ["@pip_pytest//:pkg"],  # change this to the pytest target in your repo.
        )

        deps.append(":__test__")
        main = ":__test__.py"

    _py_test(
        name = name,
        main = main,
        deps = deps,
        **kwargs,
)
```


(directive-python-library-naming-convention)=
## `python_library_naming_convention`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-binary-naming-convention)=
## `python_binary_naming_convention`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-test-naming-convention)=
## `python_test_naming_convention`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-proto-naming-convention)=
## `python_proto_naming_convention`

:::{versionadded} 1.6.0
{gh-pr}`3093`
:::

Set this directive to a string pattern to control how the generated
{bzl:obj}`py_proto_library` targets are named. When generating new
{bzl:obj}`py_proto_library` rules, Gazelle will replace `$proto_name$` in the
pattern with the name of the `proto_library` rule, stripping out a
trailing `_proto`. For example:

```starlark
# gazelle:python_generate_proto true
# gazelle:python_proto_naming_convention my_custom_$proto_name$_pattern

proto_library(
    name = "foo_proto",
    srcs = ["foo.proto"],
)
```

produces the following {bzl:obj}`py_proto_library` rule:

```starlark
py_proto_library(
    name = "my_custom_foo_pattern",
    deps = [":foo_proto"],
)
```

The default naming convention is `$proto_name$_pb2_py` in accordance with
the [Bazel `py_proto_library` convention][bazel-py-proto-library], so by default
in the above example Gazelle would generate `foo_pb2_py`. Any pre-existing
rules are left in place and not renamed.

[bazel-py-proto-library]: https://bazel.build/reference/be/protocol-buffer#py_proto_library

Note that the Python library will always be imported as `foo_pb2` in Python
code, regardless of the naming convention. Also note that Gazelle is currently
not able to map said imports, e.g. `import foo_pb2`, to fill in
{bzl:obj}`py_proto_library` targets as dependencies of other rules. See
{gh-issue}`1703`.


(directive-resolve-py)=
## `resolve py`

:::{error}
Detailed docs are not yet written.
:::


(directive-python-default-visibility)=
## `python_default_visibility`

:::{versionadded} 0.32.0
{gh-pr}`1787`
:::

Instructs gazelle to use these visibility labels on all _python_ targets
(typically `py_*`, but can be modified via the `map_kind` directive). The arg
to this directive is a comma-separated list (without spaces) of labels.

For example:

```starlark
# gazelle:python_default_visibility //:__subpackages__,//tests:__subpackages__
```

produces the following visibility attribute:

```starlark
py_library(
    ...,
    visibility = [
        "//:__subpackages__",
        "//tests:__subpackages__",
    ],
    ...,
)
```

You can also inject the `python_root` value by using the exact string
`$python_root$`. All instances of this string will be replaced by the `python_root`
value.

```starlark
# gazelle:python_default_visibility //$python_root$:__pkg__,//foo/$python_root$/tests:__subpackages__

# Assuming the "# gazelle:python_root" directive is set in ./py/src/BUILD.bazel,
# the results will be:
py_library(
    ...,
    visibility = [
        "//foo/py/src/tests:__subpackages__",  # sorted alphabetically
        "//py/src:__pkg__",
    ],
    ...,
)
```

Two special values are also accepted as an argument to the directive:

* `NONE`: This removes all default visibility. Labels added by the
  `python_visibility` directive are still included.
* `DEFAULT`: This resets the default visibility.

For example:

```starlark
# gazelle:python_default_visibility NONE

py_library(
    name = "...",
    srcs = [...],
)
```

```starlark
# gazelle:python_default_visibility //foo:bar
# gazelle:python_default_visibility DEFAULT

py_library(
    ...,
    visibility = ["//:__subpackages__"],
    ...,
)
```

These special values can be useful for sub-packages.


(directive-python-visibility)=
## `python_visibility`

:::{versionadded} 0.32.0
{gh-pr}`1784`
:::

Appends additional `visibility` labels to each generated target.

This directive can be set multiple times. The generated `visibility` attribute
will include the default visibility and all labels defined by this directive.
All labels will be ordered alphabetically.

```starlark
# ./BUILD.bazel
# gazelle:python_visibility //tests:__pkg__
# gazelle:python_visibility //bar:baz

py_library(
   ...
   visibility = [
       "//:__subpackages__",  # default visibility
       "//bar:baz",
       "//tests:__pkg__",
   ],
   ...
)
```

Child Bazel packages inherit values from parents:

```starlark
# ./bar/BUILD.bazel
# gazelle:python_visibility //tests:__subpackages__

py_library(
   ...
   visibility = [
       "//:__subpackages__",       # default visibility
       "//bar:baz",                # defined in ../BUILD.bazel
       "//tests:__pkg__",          # defined in ../BUILD.bazel
       "//tests:__subpackages__",  # defined in this ./BUILD.bazel
   ],
   ...
)

```

This directive also supports the `$python_root$` placeholder that
`# gazelle:python_default_visibility` supports.

```starlark
# gazlle:python_visibility //$python_root$/foo:bar

py_library(
    ...
    visibility = ["//this_is_my_python_root/foo:bar"],
    ...
)
```


(directive-python-test-file-pattern)=
## `python_test_file_pattern`

:::{versionadded} 0.32.0
{gh-pr}`1819`
:::

This directive adjusts which python files will be mapped to the {bzl:obj}`py_test` rule.

+ The default is `*_test.py,test_*.py`: both `test_*.py` and `*_test.py` files
  will generate {bzl:obj}`py_test` targets.
+ This directive must have a value. If no value is given, an error will be raised.
+ It is recommended, though not necessary, to include the `.py` extension in
  the {command}`glob`: `foo*.py,?at.py`.
+ Like most directives, it applies to the current Bazel package and all subpackages
  until the directive is set again.
+ This directive accepts multiple {command}`glob` patterns, separated by commas without spaces:

```starlark
# gazelle:python_test_file_pattern foo*.py,?at

py_library(
    name = "mylib",
    srcs = ["mylib.py"],
)

py_test(
    name = "foo_bar",
    srcs = ["foo_bar.py"],
)

py_test(
    name = "cat",
    srcs = ["cat.py"],
)

py_test(
    name = "hat",
    srcs = ["hat.py"],
)
```


### Notes

Resetting to the default value (such as in a subpackage) is manual. Set:

```starlark
# gazelle:python_test_file_pattern *_test.py,test_*.py
```

There currently is no way to tell gazelle that _no_ files in a package should
be mapped to {bzl:obj}`py_test` targets (see {gh-issue}`1826`). The workaround
is to set this directive to a pattern that will never match a `.py` file, such
as `foo.bar`:

```starlark
# No files in this package should be mapped to py_test targets.
# gazelle:python_test_file_pattern foo.bar

py_library(
    name = "my_test",
    srcs = ["my_test.py"],
)
```


(directive-python-label-convention)=
## `python_label_convention`

:::{versionadded} 0.34.0
{gh-pr}`1976`
:::

:::{error}
Detailed docs are not yet written.
:::


(directive-python-label-normalization)=
## `python_label_normalization`

:::{versionadded} 0.34.0
{gh-pr}`1976`
:::

:::{error}
Detailed docs are not yet written.
:::


(directive-python-experimental-allow-relative-imports)=
## `python_experimental_allow_relative_imports`

Enables experimental support for resolving relative imports in
`python_generation_mode package`.

By default, when `# gazelle:python_generation_mode package` is enabled,
relative imports (e.g., `from .library import foo`) are not added to the
deps field of the generated target. This results in incomplete {bzl:obj}`py_library`
rules that lack required dependencies on sibling packages.

Example:

Given this Python file import:

```python
from .library import add as _add
from .library import subtract as _subtract
```

Expected BUILD file output:

```starlark
py_library(
    name = "py_default_library",
    srcs = ["__init__.py"],
    deps = [
        "//example/library:py_default_library",
    ],
    visibility = ["//visibility:public"],
)
```

Actual output without this annotation:

```starlark
py_library(
    name = "py_default_library",
    srcs = ["__init__.py"],
    visibility = ["//visibility:public"],
)
```

If the directive is set to `true`, gazelle will resolve imports
that are relative to the current package.


(directive-python-generate-pyi-deps)=
## `python_generate_pyi_deps`

:::{versionadded} 1.6.0
{gh-pr}`3014`
:::

:::{error}
Detailed docs are not yet written.
:::


(directive-python-generate-pyi-srcs)=
## `python_generate_pyi_srcs`

:::{versionadded} 1.6.0
{gh-pr}`3356`
:::

When `true`, include any sibling `.pyi` files in the `pyi_srcs` target attribute.

For example, assume you have the following files:

```
foo.py
foo.pyi
```

The generated target will be:

```starlark
py_library(
    name = "foo",
    srcs = ["foo.py"],
    pyi_srcs = ["foo.pyi"],
)
```


(directive-python-generate-proto)=
## `python_generate_proto`

:::{versionadded} 1.6.0
{gh-pr}`3057`
:::

When `# gazelle:python_generate_proto true`, Gazelle will generate one
{bzl:obj}`py_proto_library` for each `proto_library`, generating Python clients for
protobuf in each package. By default this is turned off. Gazelle will also
generate a load statement for the {bzl:obj}`py_proto_library` - attempting to detect
the configured name for the `@protobuf` / `@com_google_protobuf` repo in your
`MODULE.bazel`, and otherwise falling back to `@com_google_protobuf` for
compatibility with `WORKSPACE`.

:::{note}
In order to use this, you must manually configure Gazelle to target multiple
languages. Place this in your root `BUILD.bazel` file:

```
load("@bazel_gazelle//:def.bzl", "gazelle", "gazelle_binary")

gazelle_binary(
    name = "gazelle_multilang",
    languages = [
        "@bazel_gazelle//language/proto",
        # The python gazelle plugin must be listed _after_ the proto language.
        "@rules_python_gazelle_plugin//python",
    ],
)

gazelle(
    name = "gazelle",
    gazelle = "//:gazelle_multilang",
)
```
:::


For example, in a package with `# gazelle:python_generate_proto true` and a
`foo.proto`, if you have both the proto extension and the Python extension
loaded into Gazelle, you'll get something like:

```starlark
load("@protobuf//bazel:py_proto_library.bzl", "py_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

# gazelle:python_generate_proto true

proto_library(
    name = "foo_proto",
    srcs = ["foo.proto"],
    visibility = ["//:__subpackages__"],
)

py_proto_library(
    name = "foo_py_pb2",
    visibility = ["//:__subpackages__"],
    deps = [":foo_proto"],
)
```

When `false`, Gazelle will ignore any {bzl:obj}`py_proto_library`, including
previously-generated or hand-created rules.


(directive-python-resolve-sibling-imports)=
## `python_resolve_sibling_imports`

:::{versionadded} 1.6.0
{gh-pr}`3106`
:::

:::{error}
Detailed docs are not yet written.
:::

(directive-python-include-ancestor-conftest)=
## `python_include_ancestor_conftest`

:::{versionadded} 1.9.0
{gh-pr}`3596`
:::

Version 1.9.0 includes a fix ({gh-pr}`3498`) for a long-standing issue
({gh-issue}`3497`) where ancestor `conftest.py` files were not automatically
added as dependencies of {bzl:obj}`py_test` targets.

However, some people may not want this behavior (see https://xkcd.com/1172/).
Thus the `python_include_ancestor_conftest` directive controls this behavior.
It defaults to `true`, which causes all ancestor `conftest.py` files to be
included as dependencies for {bzl:obj}`py_test` targets.

Setting the directive to `false` reverts to the pre-1.9.0 behavior.

For example, given this directory tree (not shown: intermediary `BUILD.bazel`
files)

```
./
├── conftest.py
└── one/
    ├── conftest.py
    └── two/
        ├── conftest.py
        └── three/
            ├── BUILD.bazel
            ├── conftest.py
            └── my_test.py
```

Gazelle will generate this target for `foo_test.py` by default:

```starlark
py_test(
    name = "foo_test",
    srcs = ["foo_test.py"],
    deps = [
        ":conftest",            # same as "//one:two/three:conftest"
        "//:conftest",
        "//one:conftest",
        "//one/two:conftest",
    ],
)
```

But when `python_include_ancestor_conftest` is `false`, only the sibling
`:conftest` target will be included as a dependency:

:::{tip}
The [`include_pytest_conftest` annotation](annotation-include-pytest-conftest)
controls whether the sibling `:conftest` target is added to {bzl:obj}`py_test`
target dependency list.
:::

```starlark
# gazelle:python_include_ancestor_conftest false
py_test(
    name = "foo_test",
    srcs = ["foo_test.py"],
    deps = [
        ":conftest",
    ],
)
```
