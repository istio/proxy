:::{default-domain} bzl
:::

# Using dependencies from PyPI

Using PyPI packages (aka "pip install") involves two main steps.

1. [Installing third party packages](#installing-third-party-packages)
2. [Using third party packages as dependencies](#using-third-party-packages)

{#installing-third-party-packages}
## Installing third party packages

### Using bzlmod

To add pip dependencies to your `MODULE.bazel` file, use the `pip.parse`
extension, and call it to create the central external repo and individual wheel
external repos. Include in the `MODULE.bazel` the toolchain extension as shown
in the first bzlmod example above.

```starlark
pip = use_extension("@rules_python//python/extensions:pip.bzl", "pip")
pip.parse(
    hub_name = "my_deps",
    python_version = "3.11",
    requirements_lock = "//:requirements_lock_3_11.txt",
)
use_repo(pip, "my_deps")
```
For more documentation, including how the rules can update/create a requirements
file, see the bzlmod examples under the {gh-path}`examples` folder or the documentation
for the {obj}`@rules_python//python/extensions:pip.bzl` extension.

```{note}
We are using a host-platform compatible toolchain by default to setup pip dependencies.
During the setup phase, we create some symlinks, which may be inefficient on Windows
by default. In that case use the following `.bazelrc` options to improve performance if
you have admin privileges:

    startup --windows_enable_symlinks

This will enable symlinks on Windows and help with bootstrap performance of setting up the 
hermetic host python interpreter on this platform. Linux and OSX users should see no
difference.
```

### Using a WORKSPACE file

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

(vendoring-requirements)=
#### Vendoring the requirements.bzl file

In some cases you may not want to generate the requirements.bzl file as a repository rule
while Bazel is fetching dependencies. For example, if you produce a reusable Bazel module
such as a ruleset, you may want to include the requirements.bzl file rather than make your users
install the WORKSPACE setup to generate it.
See https://github.com/bazelbuild/rules_python/issues/608

This is the same workflow as Gazelle, which creates `go_repository` rules with
[`update-repos`](https://github.com/bazelbuild/bazel-gazelle#update-repos)

To do this, use the "write to source file" pattern documented in
https://blog.aspect.dev/bazel-can-write-to-the-source-folder
to put a copy of the generated requirements.bzl into your project.
Then load the requirements.bzl file directly rather than from the generated repository.
See the example in rules_python/examples/pip_parse_vendored.

(per-os-arch-requirements)=
### Requirements for a specific OS/Architecture

In some cases you may need to use different requirements files for different OS, Arch combinations. This is enabled via the `requirements_by_platform` attribute in `pip.parse` extension and the `pip_parse` repository rule. The keys of the dictionary are labels to the file and the values are a list of comma separated target (os, arch) tuples.

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

In case of duplicate platforms, `rules_python` will raise an error as there has
to be unambiguous mapping of the requirement files to the (os, arch) tuples.

An alternative way is to use per-OS requirement attributes.
```starlark
    # ...
    requirements_windows = "requirements_windows.txt",
    requirements_darwin = "requirements_darwin.txt",
    # For the remaining platforms (which is basically only linux OS), use this file.
    requirements_lock = "requirements_lock.txt",
)
```

### pip rules

Note that since `pip_parse` and `pip.parse` are executed at evaluation time,
Bazel has no information about the Python toolchain and cannot enforce that the
interpreter used to invoke `pip` matches the interpreter used to run
`py_binary` targets. By default, `pip_parse` uses the system command
`"python3"`. To override this, pass in the `python_interpreter` attribute or
`python_interpreter_target` attribute to `pip_parse`. The `pip.parse` `bzlmod` extension
by default uses the hermetic python toolchain for the host platform.

You can have multiple `pip_parse`s in the same workspace, or use the pip
extension multiple times when using bzlmod. This configuration will create
multiple external repos that have no relation to one another and may result in
downloading the same wheels numerous times.

As with any repository rule, if you would like to ensure that `pip_parse` is
re-executed to pick up a non-hermetic change to your environment (e.g., updating
your system `python` interpreter), you can force it to re-execute by running
`bazel sync --only [pip_parse name]`.

{#using-third-party-packages}
## Using third party packages as dependencies

Each extracted wheel repo contains a `py_library` target representing
the wheel's contents. There are two ways to access this library. The
first uses the `requirement()` function defined in the central
repo's `//:requirements.bzl` file. This function maps a pip package
name to a label:

```starlark
load("@my_deps//:requirements.bzl", "requirement")

py_library(
    name = "mylib",
    srcs = ["mylib.py"],
    deps = [
        ":myotherlib",
        requirement("some_pip_dep"),
        requirement("another_pip_dep"),
    ]
)
```

The reason `requirement()` exists is to insulate from
changes to the underlying repository and label strings. However, those
labels have become directly used, so aren't able to easily change regardless.

On the other hand, using `requirement()` has several drawbacks; see
[this issue][requirements-drawbacks] for an enumeration. If you don't
want to use `requirement()`, you can use the library
labels directly instead. For `pip_parse`, the labels are of the following form:

```starlark
@{name}//{package}
```

Here `name` is the `name` attribute that was passed to `pip_parse` and
`package` is the pip package name with characters that are illegal in
Bazel label names (e.g. `-`, `.`) replaced with `_`. If you need to
update `name` from "old" to "new", then you can run the following
buildozer command:

```shell
buildozer 'substitute deps @old//([^/]+) @new//${1}' //...:*
```

[requirements-drawbacks]: https://github.com/bazelbuild/rules_python/issues/414

### Entry points

If you would like to access [entry points][whl_ep], see the `py_console_script_binary` rule documentation,
which can help you create a `py_binary` target for a particular console script exposed by a package.

[whl_ep]: https://packaging.python.org/specifications/entry-points/

### 'Extras' dependencies

Any 'extras' specified in the requirements lock file will be automatically added
as transitive dependencies of the package. In the example above, you'd just put
`requirement("useful_dep")` or `@pypi//useful_dep`.

### Consuming Wheel Dists Directly

If you need to depend on the wheel dists themselves, for instance, to pass them
to some other packaging tool, you can get a handle to them with the
`whl_requirement` macro. For example:

```starlark
load("@pypi//:requirements.bzl", "whl_requirement")

filegroup(
    name = "whl_files",
    data = [
        # This is equivalent to "@pypi//boto3:whl"
        whl_requirement("boto3"),
    ]
)
```

### Creating a filegroup of files within a whl

The rule {obj}`whl_filegroup` exists as an easy way to extract the necessary files
from a whl file without the need to modify the `BUILD.bazel` contents of the
whl repositories generated via `pip_repository`. Use it similarly to the `filegroup`
above. See the API docs for more information.

(advance-topics)=
## Advanced topics

(circular-deps)=
### Circular dependencies

Sometimes PyPi packages contain dependency cycles -- for instance a particular
version `sphinx` (this is no longer the case in the latest version as of
2024-06-02) depends on `sphinxcontrib-serializinghtml`. When using them as
`requirement()`s, ala

```
py_binary(
    name = "doctool",
    ...
    deps = [
        requirement("sphinx"),
    ],
)
```

Bazel will protest because it doesn't support cycles in the build graph --

```
ERROR: .../external/pypi_sphinxcontrib_serializinghtml/BUILD.bazel:44:6: in alias rule @pypi_sphinxcontrib_serializinghtml//:pkg: cycle in dependency graph:
    //:doctool (...)
    @pypi//sphinxcontrib_serializinghtml:pkg (...)
.-> @pypi_sphinxcontrib_serializinghtml//:pkg (...)
|   @pypi_sphinxcontrib_serializinghtml//:_pkg (...)
|   @pypi_sphinx//:pkg (...)
|   @pypi_sphinx//:_pkg (...)
`-- @pypi_sphinxcontrib_serializinghtml//:pkg (...)
```

The `experimental_requirement_cycles` argument allows you to work around these
issues by specifying groups of packages which form cycles. `pip_parse` will
transparently fix the cycles for you and provide the cyclic dependencies
simultaneously.

```starlark
pip_parse(
    ...
    experimental_requirement_cycles = {
        "sphinx": [
            "sphinx",
            "sphinxcontrib-serializinghtml",
        ]
    },
)
```

`pip_parse` supports fixing multiple cycles simultaneously, however cycles must
be distinct. `apache-airflow` for instance has dependency cycles with a number
of its optional dependencies, which means those optional dependencies must all
be a part of the `airflow` cycle. For instance --

```starlark
pip_parse(
    ...
    experimental_requirement_cycles = {
        "airflow": [
            "apache-airflow",
            "apache-airflow-providers-common-sql",
            "apache-airflow-providers-postgres",
            "apache-airflow-providers-sqlite",
        ]
    }
)
```

Alternatively, one could resolve the cycle by removing one leg of it.

For example while `apache-airflow-providers-sqlite` is "baked into" the Airflow
package, `apache-airflow-providers-postgres` is not and is an optional feature.
Rather than listing `apache-airflow[postgres]` in your `requirements.txt` which
would expose a cycle via the extra, one could either _manually_ depend on
`apache-airflow` and `apache-airflow-providers-postgres` separately as
requirements. Bazel rules which need only `apache-airflow` can take it as a
dependency, and rules which explicitly want to mix in
`apache-airflow-providers-postgres` now can.

Alternatively, one could use `rules_python`'s patching features to remove one
leg of the dependency manually. For instance by making
`apache-airflow-providers-postgres` not explicitly depend on `apache-airflow` or
perhaps `apache-airflow-providers-common-sql`.


(bazel-downloader)=
### Multi-platform support

Multi-platform support of cross-building the wheels can be done in two ways - either
using {bzl:attr}`experimental_index_url` for the {bzl:obj}`pip.parse` bzlmod tag class
or by using the {bzl:attr}`pip.parse.download_only` setting. In this section we
are going to outline quickly how one can use the latter option.

Let's say you have 2 requirements files:
```
# requirements.linux_x86_64.txt
--platform=manylinux_2_17_x86_64
--python-version=39
--implementation=cp
--abi=cp39

foo==0.0.1 --hash=sha256:deadbeef
bar==0.0.1 --hash=sha256:deadb00f
```

```
# requirements.osx_aarch64.txt contents
--platform=macosx_10_9_arm64
--python-version=39
--implementation=cp
--abi=cp39

foo==0.0.3 --hash=sha256:deadbaaf
```

With these 2 files your {bzl:obj}`pip.parse` could look like:
```
pip.parse(
    hub_name = "pip",
    python_version = "3.9",
    # Tell `pip` to ignore sdists
    download_only = True,
    requirements_by_platform = {
        "requirements.linux_x86_64.txt": "linux_x86_64",
        "requirements.osx_aarch64.txt": "osx_aarch64",
    },
)
```

With this, the `pip.parse` will create a hub repository that is going to
support only two platforms - `cp39_osx_aarch64` and `cp39_linux_x86_64` and it
will only use `wheels` and ignore any sdists that it may find on the PyPI
compatible indexes.

```{note}
This is only supported on `bzlmd`.
```

(bazel-downloader)=
### Bazel downloader and multi-platform wheel hub repository.

The `bzlmod` `pip.parse` call supports pulling information from `PyPI` (or a
compatible mirror) and it will ensure that the [bazel
downloader][bazel_downloader] is used for downloading the wheels. This allows
the users to use the [credential helper](#credential-helper) to authenticate
with the mirror and it also ensures that the distribution downloads are cached.
It also avoids using `pip` altogether and results in much faster dependency
fetching.

This can be enabled by `experimental_index_url` and related flags as shown in
the {gh-path}`examples/bzlmod/MODULE.bazel` example.

When using this feature during the `pip` extension evaluation you will see the accessed indexes similar to below:
```console
Loading: 0 packages loaded
    currently loading: docs/
    Fetching module extension pip in @@//python/extensions:pip.bzl; starting
    Fetching https://pypi.org/simple/twine/
```

This does not mean that `rules_python` is fetching the wheels eagerly, but it
rather means that it is calling the PyPI server to get the Simple API response
to get the list of all available source and wheel distributions. Once it has
got all of the available distributions, it will select the right ones depending
on the `sha256` values in your `requirements_lock.txt` file. The compatible
distribution URLs will be then written to the `MODULE.bazel.lock` file. Currently
users wishing to use the lock file with `rules_python` with this feature have
to set an environment variable `RULES_PYTHON_OS_ARCH_LOCK_FILE=0` which will
become default in the next release.

Fetching the distribution information from the PyPI allows `rules_python` to
know which `whl` should be used on which target platform and it will determine
that by parsing the `whl` filename based on [PEP600], [PEP656] standards. This
allows the user to configure the behaviour by using the following publicly
available flags:
* {obj}`--@rules_python//python/config_settings:py_linux_libc` for selecting the Linux libc variant.
* {obj}`--@rules_python//python/config_settings:pip_whl` for selecting `whl` distribution preference.
* {obj}`--@rules_python//python/config_settings:pip_whl_osx_arch` for selecting MacOS wheel preference.
* {obj}`--@rules_python//python/config_settings:pip_whl_glibc_version` for selecting the GLIBC version compatibility.
* {obj}`--@rules_python//python/config_settings:pip_whl_muslc_version` for selecting the musl version compatibility.
* {obj}`--@rules_python//python/config_settings:pip_whl_osx_version` for selecting MacOS version compatibility.

[bazel_downloader]: https://bazel.build/rules/lib/builtins/repository_ctx#download
[pep600]: https://peps.python.org/pep-0600/
[pep656]: https://peps.python.org/pep-0656/

(credential-helper)=
### Credential Helper

The "use Bazel downloader for python wheels" experimental feature includes support for the Bazel
[Credential Helper][cred-helper-design].

Your python artifact registry may provide a credential helper for you. Refer to your index's docs
to see if one is provided.

See the [Credential Helper Spec][cred-helper-spec] for details.

[cred-helper-design]: https://github.com/bazelbuild/proposals/blob/main/designs/2022-06-07-bazel-credential-helpers.md
[cred-helper-spec]: https://github.com/EngFlow/credential-helper-spec/blob/main/spec.md


#### Basic Example:

The simplest form of a credential helper is a bash script that accepts an arg and spits out JSON to
stdout. For a service like Google Artifact Registry that uses ['Basic' HTTP Auth][rfc7617] and does
not provide a credential helper that conforms to the [spec][cred-helper-spec], the script might
look like:

```bash
#!/bin/bash
# cred_helper.sh
ARG=$1  # but we don't do anything with it as it's always "get"

# formatting is optional
echo '{'
echo '  "headers": {'
echo '    "Authorization": ["Basic dGVzdDoxMjPCow=="]'
echo '  }'
echo '}'
```

Configure Bazel to use this credential helper for your python index `example.com`:

```
# .bazelrc
build --credential_helper=example.com=/full/path/to/cred_helper.sh
```

Bazel will call this file like `cred_helper.sh get` and use the returned JSON to inject headers
into whatever HTTP(S) request it performs against `example.com`.

[rfc7617]: https://datatracker.ietf.org/doc/html/rfc7617
