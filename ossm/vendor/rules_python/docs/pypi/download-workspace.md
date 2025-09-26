:::{default-domain} bzl
:::

# Download (WORKSPACE)

This documentation page covers how to download PyPI dependencies in the legacy `WORKSPACE` setup.

To add pip dependencies to your `WORKSPACE`, load the `pip_parse` function and
call it to create the central external repo and individual wheel external repos.

```starlark
load("@rules_python//python:pip.bzl", "pip_parse")

# Create a central repo that knows about the dependencies needed from
# requirements_lock.txt.
pip_parse(
   name = "my_deps",
   requirements_lock = "//path/to:requirements_lock.txt",
)

# Load the starlark macro, which will define your dependencies.
load("@my_deps//:requirements.bzl", "install_deps")

# Call it to define repos for your requirements.
install_deps()
```

## Interpreter selection

Note that because `pip_parse` runs before Bazel decides which Python toolchain to use, it cannot
enforce that the interpreter used to invoke `pip` matches the interpreter used to run `py_binary`
targets. By default, `pip_parse` uses the system command `"python3"`. To override this, pass in the
{attr}`pip_parse.python_interpreter` attribute or {attr}`pip_parse.python_interpreter_target`.

You can have multiple `pip_parse`s in the same workspace. This configuration will create multiple
external repos that have no relation to one another and may result in downloading the same wheels
numerous times.

As with any repository rule, if you would like to ensure that `pip_parse` is
re-executed to pick up a non-hermetic change to your environment (e.g., updating
your system `python` interpreter), you can force it to re-execute by running
`bazel sync --only [pip_parse name]`.

(per-os-arch-requirements)=
## Requirements for a specific OS/Architecture

In some cases, you may need to use different requirements files for different OS and architecture combinations.
This is enabled via the {attr}`pip_parse.requirements_by_platform` attribute. The keys of the
dictionary are labels to the file, and the values are a list of comma-separated target (os, arch)
tuples.

For example:
```starlark
    # ...
    requirements_by_platform = {
        "requirements_linux_x86_64.txt": "linux_x86_64",
        "requirements_osx.txt": "osx_*",
        "requirements_linux_exotic.txt": "linux_exotic",
        "requirements_some_platforms.txt": "linux_aarch64,windows_*",
    },
    # For the list of standard platforms that the rules_python has toolchains for, default to
    # the following requirements file.
    requirements_lock = "requirements_lock.txt",
```

In case of duplicate platforms, `rules_python` will raise an error, as there has
to be an unambiguous mapping of the requirement files to the (os, arch) tuples.

An alternative way is to use per-OS requirement attributes.
```starlark
    # ...
    requirements_windows = "requirements_windows.txt",
    requirements_darwin = "requirements_darwin.txt",
    # For the remaining platforms (which is basically only linux OS), use this file.
    requirements_lock = "requirements_lock.txt",
)
```

:::{note}
If you are using a universal lock file but want to restrict the list of platforms that
the lock file will be evaluated against, consider using the aforementioned
`requirements_by_platform` attribute and listing the platforms explicitly.
:::

(vendoring-requirements)=
## Vendoring the requirements.bzl file

:::{note}
For `bzlmod`, refer to standard `bazel vendor` usage if you want to really vendor it, otherwise
just use the `pip` extension as you would normally.

However, be aware that there are caveats when doing so.
:::

In some cases you may not want to generate the requirements.bzl file as a repository rule
while Bazel is fetching dependencies. For example, if you produce a reusable Bazel module
such as a ruleset, you may want to include the `requirements.bzl` file rather than make your users
install the `WORKSPACE` setup to generate it, see {gh-issue}`608`.

This is the same workflow as Gazelle, which creates `go_repository` rules with
[`update-repos`](https://github.com/bazelbuild/bazel-gazelle#update-repos)

To do this, use the "write to source file" pattern documented in
<https://blog.aspect.dev/bazel-can-write-to-the-source-folder>
to put a copy of the generated `requirements.bzl` into your project.
Then load the requirements.bzl file directly rather than from the generated repository.
See the example in {gh-path}`examples/pip_parse_vendored`.
