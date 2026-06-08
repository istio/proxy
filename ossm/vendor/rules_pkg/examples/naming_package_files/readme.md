# Examples of how to name packages using build time configuration.

## Examples

The examples below only show snippets of the relevant technique.
See the BUILD file for the complete source.

### Using command line flags to modify a package name

We can use a `config_setting` to capture the command line flag and then
`select()` on that to drop a part into into the name.

```python
config_setting(
    name = "special_build",
    values = {"define": "SPECIAL=1"},
)

my_package_naming(
    name = "my_naming_vars",
    special_build = select({
        ":special_build": "-IsSpecial",
        "//conditions:default": "",
    }),
)
```

```shell
bazel build :example1
ls -l bazel-bin/example1.tar bazel-bin/RulesPkgExamples-k8-fastbuild.tar
```

```shell
bazel build :example1 --define=SPECIAL=1
ls -l bazel-bin/example1*.tar
```

### Using values from a toolchain in a package name.

The rule providing the naming can depend on toolchains just like a `*_library`
or `*_binary` rule

```python
def _names_from_toolchains_impl(ctx):
    values = {}
    cc_toolchain = find_cc_toolchain(ctx)
    values['cc_cpu'] = cc_toolchain.cpu
    return PackageVariablesInfo(values = values)

names_from_toolchains = rule(
    implementation = _names_from_toolchains_impl,
    attrs = {
        "_cc_toolchain": attr.label(
            default = Label(
                "@rules_cc//cc:current_cc_toolchain",
            ),
        ),
    },
    toolchains = ["@rules_cc//cc:toolchain_type"],
)
```

```shell
bazel build :example2
ls -l bazel-bin/example2*.tar
```

### Debian package names

Debian package names are of the form `<package>_<version>-<revision>_<arch>.deb`.

One way you might do that is shown in this snipped from the `BUILD` file.

```python
VERSION = "1"
REVISION = "2"

basic_naming(
    name = "my_naming_vars",

    version = VERSION,
    revision = REVISION,
    ...
)

pkg_deb(
    name = "a_deb_package",
    package = "foo-tools",
    ...
    # Note: target_cpu comes from the --cpu on the command line, and does not
    # have to be stated in the BUILD file.
    package_file_name = "foo-tools_{version}-{revision}_{target_cpu}.deb",
    package_variables = ":my_naming_vars",
    version = VERSION,
)
```

Try building `bazel build :a_deb_package` then examine the results. Note that
the .deb out file has the correctly formed name, while the target itself is
a symlink to that file.

```console
$ ls -l bazel-bin/a_deb_package.deb bazel-bin/foo-tools_1-2_k8.deb
lrwxrwxrwx 1 user primarygroup   163 Jul 26 12:56 bazel-bin/a_deb_package.deb -> /home/user/.cache/bazel/_bazel_user/.../execroot/rules_pkg_examples/bazel-out/k8-fastbuild/bin/foo-tools_1-2_k8.deb
-r-xr-xr-x 1 user primarygroup 10662 Jul 26 12:56 bazel-bin/foo-tools_1-2_k8.deb
```
