# rules_jvm_external

Transitive Maven artifact resolution and publishing rules for Bazel.

* Main build: [![Build
Status](https://badge.buildkite.com/26d895f5525652e57915a607d0ecd3fc945c8280a0bdff83d9.svg?branch=master)](https://buildkite.com/bazel/rules-jvm-external)
* Examples build: [![Build status](https://badge.buildkite.com/d1e93c6c5321c9f7d24c71d849f00542f4ac5c9eed763eca2d.svg)](https://buildkite.com/bazel/rules-jvm-external-examples)


Table of Contents
=================

- [Features](#features)
- [Examples](#examples)
    - [Projects using rules_jvm_external](#projects-using-rules_jvm_external)
- [Prerequisites](#prerequisites)
- [Usage](#usage)
    - [With bzlmod (Bazel 7 and above)](#with-bzlmod-bazel-7-and-above)
    - [With WORKSPACE file (legacy)](#with-workspace-file-legacy)
- [API Reference](#api-reference)
- [Pinning artifacts and integration with Bazel's downloader](#pinning-artifacts-and-integration-with-bazels-downloader)
    - [Updating `maven_install.json`](#updating-maven_installjson)
    - [Requiring lock file repinning when the list of artifacts changes](#requiring-lock-file-repinning-when-the-list-of-artifacts-changes)
    - [Custom location for `maven_install.json`](#custom-location-for-maven_installjson)
    - [Multiple `maven_install.json` files](#multiple-maven_installjson-files)
- [(Experimental) Support for Maven BOM files](#experimental-support-for-maven-bom-files)
- [Generated targets](#generated-targets)
- [Outdated artifacts](#outdated-artifacts)
- [Advanced usage](#advanced-usage)
    - [Fetch source JARs](#fetch-source-jars)
    - [Checksum verification](#checksum-verification)
    - [Using a custom Coursier download url](#using-a-custom-coursier-download-url)
    - [`artifact` helper macro](#artifact-helper-macro)
    - [`java_plugin_artifact` helper macro](#java_plugin_artifact-helper-macro)
    - [Multiple `maven_install` declarations for isolated artifact version trees](#multiple-maven_install-declarations-for-isolated-artifact-version-trees)
    - [Detailed dependency information specifications](#detailed-dependency-information-specifications)
    - [Artifact exclusion](#artifact-exclusion)
    - [Compile-only dependencies](#compile-only-dependencies)
    - [Test-only dependencies](#test-only-dependencies)
    - [Resolving user-specified and transitive dependency version conflicts](#resolving-user-specified-and-transitive-dependency-version-conflicts)
    - [Overriding generated targets](#overriding-generated-targets)
    - [Proxies](#proxies)
    - [Repository aliases](#repository-aliases)
    - [Repository remapping](#repository-remapping)
    - [Hiding transitive dependencies](#hiding-transitive-dependencies)
    - [Accessing transitive dependencies list](#accessing-transitive-dependencies-list)
    - [Fetch and resolve timeout](#fetch-and-resolve-timeout)
    - [Ignoring empty jars](#ignoring-empty-jars)
    - [Duplicate artifact warning](#duplicate-artifact-warning)
    - [Provide JVM options for artifact resolution](#provide-jvm-options-for-artifact-resolution)
    - [Provide JVM options for Coursier with `COURSIER_OPTS`](#provide-jvm-options-for-coursier-with-coursier_opts)
    - [Resolving issues with nonstandard system default JDKs](#resolving-issues-with-nonstandard-system-default-jdks)
    - [Exporting and consuming artifacts from external repositories](#exporting-and-consuming-artifacts-from-external-repositories)
    - [Publishing to External Repositories](#publishing-to-external-repositories)
- [Configuring the dependency resolver](#configuring-the-dependency-resolver)
    - [Common options](#common-options)
    - [Configuring Coursier](#configuring-coursier)
    - [Configuring Maven](#configuring-maven)
- [IPv6 support](#ipv6-support)
- [Developing this project](#developing-this-project)
    - [Verbose / debug mode](#verbose--debug-mode)
    - [Tests](#tests)
    - [Installing the Android SDK on macOS](#installing-the-android-sdk-on-macos)
    - [Generating documentation](#generating-documentation)

## Features

* MODULE.bazel bzlmod configuration (Bazel 7 and above) 
* WORKSPACE configuration
* Artifact version resolution with Coursier, Maven or Gradle
* Import downloaded JAR, AAR, source JARs
* Export built JARs to Maven repositories
* Pin resolved artifacts with their SHA-256 checksums into a version-controllable JSON file
* Custom Maven repositories
* Private Maven repositories using `netrc` files
* Integration with Bazel's downloader and caching mechanisms for sharing artifacts across Bazel workspaces
* Versionless target labels for simpler dependency management
* Ability to declare multiple sets of versioned artifacts
* Supported on Windows, macOS, Linux

Get the [latest release
here](https://github.com/bazelbuild/rules_jvm_external/releases/latest).

## Examples

You can find examples in the [`examples/`](./examples/) directory.

### Projects using rules_jvm_external

Find other GitHub projects using `rules_jvm_external`
[with this search query](https://github.com/search?p=1&q=rules_jvm_external+filename%3A%2FWORKSPACE+filename%3A%5C.bzl&type=Code).


## Prerequisites

* Bazel 6.4.0, up to the current LTS version.
* Support for Bazel versions between `5.4` and `7.x` is only available on releases `6.x`.
* Support for Bazel versions between `4.x` and `5.4` is only available on releases `5.x`.
* Support for Bazel versions before `4.0.0` is only available on releases `4.2` or earlier.

**Compatibility guideline:** This project aims to be backwards compatible with
the (current LTS - 2) version. If the current LTS version is 8, then we aim to
support versions 6, 7 and 8.

## Usage

### With bzlmod (Bazel 7 and above)

If you are starting a new project, or your project is already using Bazel 7 and
above, we recommend using [`bzlmod`](https://bazel.build/external/overview) to
manage your external dependencies, including Maven dependencies with
`rules_jvm_external`. It address several shortcomings of the `WORKSPACE`
mechanism. If you are unable to use `bzlmod`, `rules_jvm_external` also supports
the `WORKSPACE` mechanism (see below).

See [bzlmod.md](./docs/bzlmod.md) for the usage instructions. bzlmod is
on-by-default in Bazel 7.0.

### With WORKSPACE file (legacy)

NOTE: WORKSPACE support is disabled by default in Bazel 8.0, and will be removed in Bazel 9.0.

List the top-level Maven artifacts and servers in the WORKSPACE:

```python
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

RULES_JVM_EXTERNAL_TAG = "4.5"
RULES_JVM_EXTERNAL_SHA = "b17d7388feb9bfa7f2fa09031b32707df529f26c91ab9e5d909eb1676badd9a6"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = RULES_JVM_EXTERNAL_SHA,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "junit:junit:4.12",
        "androidx.test.espresso:espresso-core:3.1.1",
        "org.hamcrest:hamcrest-library:1.3",
    ],
    repositories = [
        # Private repositories are supported through HTTP Basic auth
        "http://username:password@localhost:8081/artifactory/my-repository",
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ],
)
```

Credentials for private repositories can also be specified using a property file
or environment variables. See the [Coursier
documentation](https://get-coursier.io/docs/other-credentials.html#property-file)
for more information.

`rules_jvm_external_deps` uses a default list of maven repositories to download
 `rules_jvm_external`'s own dependencies from. Should you wish to change this,
 use the `repositories` parameter, and also set the path to the lock file:

 ```python
rules_jvm_external_deps(
    repositories = ["https://mycorp.com/artifacts"],
    deps_lock_file = "@//:rules_jvm_external_deps_install.json")
rules_jvm_external_setup()
```

If you are using `bzlmod`, define an `install` tag in your root
`MODULE.bazel` which overrides the values:

```python
maven.install(
    name = "rules_jvm_external_deps",
    repositories = ["https://mycorp.com/artifacts"],
    lock_file = "//:rules_jvm_external_deps_install.json",
)
```

Once these changes have been made, repin using `REPIN=1 bazel run
@rules_jvm_external_deps//:pin` and commit the file to your version
control system (note that at this point you will need to maintain your
customized `rules_jvm_external_deps_install.json`):

Next, reference the artifacts in the BUILD file with their versionless label:

```python
java_library(
    name = "java_test_deps",
    exports = [
        "@maven//:junit_junit",
        "@maven//:org_hamcrest_hamcrest_library",
    ],
)

android_library(
    name = "android_test_deps",
    exports = [
        "@maven//:junit_junit",
        "@maven//:androidx_test_espresso_espresso_core",
    ],
)
```

The default label syntax for an artifact `foo.bar:baz-qux:1.2.3` is `@maven//:foo_bar_baz_qux`. That is,

* All non-alphanumeric characters are substituted with underscores.
* Only the group and artifact IDs are required.
* The target is located in the `@maven` top level package (`@maven//`).

## API Reference

You can find the complete API reference at [docs/api.md](docs/api.md).

## Pinning artifacts and integration with Bazel's downloader

`rules_jvm_external` supports pinning artifacts and their SHA-256 checksums into
a `maven_install.json` file that can be checked into your repository.

Without artifact pinning, in a clean checkout of your project, `rules_jvm_external`
executes the full artifact resolution and fetching steps (which can take a bit of time)
and does not verify the integrity of the artifacts against their checksums. The
downloaded artifacts also cannot be shared across Bazel workspaces.

By pinning artifact versions, you can get improved artifact resolution and build times,
since using `maven_install.json` enables `rules_jvm_external` to integrate with Bazel's
downloader that caches files on their sha256 checksums. It also improves resiliency and
integrity by tracking the sha256 checksums and original artifact urls in the
JSON file.

Since all artifacts are persisted locally in Bazel's cache, it means that
**fully offline builds are possible** after the initial `bazel fetch @maven//...`.
The artifacts are downloaded with `http_file` which supports `netrc` for authentication.
Your `~/.netrc` will be included automatically.
To pass machine login credentials in the ~/.netrc file to coursier, specify
`use_credentials_from_home_netrc_file = True` in your `maven_install` rule.
For additional credentials, add them in the repository URLs passed to `maven_install`
(so they will be included in the generated JSON).
Alternatively, pass an array of `additional_netrc_lines` to `maven_install` for authentication with credentials from
outside the workspace.

To get started with pinning artifacts, run the following command to generate the
initial `maven_install.json` at the root of your Bazel workspace:

```
$ bazel run @maven//:pin
```

Then, specify `maven_install_json` in `maven_install` and load
`pinned_maven_install` from `@maven//:defs.bzl`:

```python
maven_install(
    # artifacts, repositories, ...
    maven_install_json = "//:maven_install.json",
)

load("@maven//:defs.bzl", "pinned_maven_install")
pinned_maven_install()
```

**Note:** The `//:maven_install.json` label assumes you have a BUILD file in
your project's root directory. If you do not have one, create an empty BUILD
file to fix issues you may see. See
[#242](https://github.com/bazelbuild/rules_jvm_external/issues/242)

**Note:** If you're using an older version of `rules_jvm_external` and
haven't repinned your dependencies, you may see a warning that you lock
file "does not contain a signature of the required artifacts" then don't
worry: either ignore the warning or repin the dependencies.

### Updating `maven_install.json`

Whenever you make a change to the list of `artifacts` or `repositories` and want
to update `maven_install.json`, run this command to re-pin the unpinned `@maven`
repository:

```
$ REPIN=1 bazel run @maven//:pin
```

Without re-pinning, `maven_install` will not pick up the changes made to the
WORKSPACE, as `maven_install.json` is now the source of truth.

### Requiring lock file repinning when the list of artifacts changes

It can be easy to forget to update the `maven_install.json` lock file
when updating artifacts in a `maven_install`. Normally,
rules_jvm_external will print a warning to the console and continue
the build when this happens, but by setting the
`fail_if_repin_required` attribute to `True`, this will be treated as
a build error, causing the build to fail. When this attribute is set,
it is possible to update the `maven_install.json` file using:

```shell
# To repin everything:
REPIN=1 bazel run @maven//:pin

# To only repin rules_jvm_external:
RULES_JVM_EXTERNAL_REPIN=1 bazel run @maven//:pin
```

Alternatively, it is also possible to modify the
`fail_if_repin_required` attribute in your `WORKSPACE` file, run
`bazel run @maven//:pin` and then reset the
`fail_if_repin_required` attribute.

### Custom location for `maven_install.json`

You can specify a custom location for `maven_install.json` by changing the
`maven_install_json` attribute value to point to the new file label. For example:

```python
maven_install(
    name = "maven_install_in_custom_location",
    artifacts = ["com.google.guava:guava:27.0-jre"],
    repositories = ["https://repo1.maven.org/maven2"],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:maven_install.json",
)

load("@maven_install_in_custom_location//:defs.bzl", "pinned_maven_install")
pinned_maven_install()
```

Future artifact pinning updates to `maven_install.json` will overwrite the file
at the specified path instead of creating a new one at the default root
directory location.

### Multiple `maven_install.json` files

If you have multiple `maven_install` declarations, you have to alias
`pinned_maven_install` to another name to prevent redefinitions:

```python
maven_install(
    name = "foo",
    maven_install_json = "//:foo_maven_install.json",
    # ...
)

load("@foo//:defs.bzl", foo_pinned_maven_install = "pinned_maven_install")
foo_pinned_maven_install()

maven_install(
    name = "bar",
    maven_install_json = "//:bar_maven_install.json",
    # ...
)

load("@bar//:defs.bzl", bar_pinned_maven_install = "pinned_maven_install")
bar_pinned_maven_install()
```

## (Experimental) Support for Maven BOM files

Maven BOMs can be used by using the `boms` attribute, for example:

```starlark
maven.install(
    boms = [
        "org.seleniumhq.selenium:selenium-bom:4.18.1",
    ],
    artifacts = [
        # This dependency is included in the `selenium-bom`, so we can omit the version number
        "org.seleniumhq.selenium:selenium-java",
    ],
)
```

## Generated targets

For the `junit:junit` example, using `bazel query @maven//:all --output=build`, we can see that the rule generated these targets:

```python
alias(
  name = "junit_junit_4_12",
  actual = "@maven//:junit_junit",
)

jvm_import(
  name = "junit_junit",
  jars = ["@maven//:https/repo1.maven.org/maven2/junit/junit/4.12/junit-4.12.jar"],
  srcjar = "@maven//:https/repo1.maven.org/maven2/junit/junit/4.12/junit-4.12-sources.jar",
  deps = ["@maven//:org_hamcrest_hamcrest_core"],
  tags = ["maven_coordinates=junit:junit:4.12"],
)

jvm_import(
  name = "org_hamcrest_hamcrest_core",
  jars = ["@maven//:https/repo1.maven.org/maven2/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar"],
  srcjar = "@maven//:https/repo1.maven.org/maven2/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3-sources.jar",
  deps = [],
  tags = ["maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
)
```

These targets can be referenced by:

*   `@maven//:junit_junit`
*   `@maven//:org_hamcrest_hamcrest_core`

**Transitive classes**: To use a class from `hamcrest-core` in your test, it's not sufficient to just
depend on `@maven//:junit_junit` even though JUnit depends on Hamcrest. The compile classes are not exported
transitively, so your test should also depend on `@maven//:org_hamcrest_hamcrest_core`.

**Original coordinates**: The generated `tags` attribute value also contains the original coordinates of
the artifact, which integrates with rules like [bazel-common's
`pom_file`](https://github.com/google/bazel-common/blob/f1115e0f777f08c3cdb115526c4e663005bec69b/tools/maven/pom_file.bzl#L177)
for generating POM files. See the [`pom_file_generation`
example](examples/pom_file_generation/) for more information.

## Outdated artifacts

To check for updates of artifacts, run the following command at the root of your Bazel workspace:

```
$ bazel run @maven//:outdated
```

## Advanced usage

### Fetch source JARs

To download the source JAR alongside the main artifact JAR, set `fetch_sources =
True` in `maven_install`:

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    fetch_sources = True,
)
```

### Checksum verification

Artifact resolution will fail if a `SHA-1` or `MD5` checksum file for the
artifact is missing in the repository. To disable this behavior, set
`fail_on_missing_checksum = False` in `maven_install`:

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    fail_on_missing_checksum = False,
)
```

### Using a custom Coursier download url

By default bazel bootstraps Coursier via [the urls specificed in versions.bzl](private/versions.bzl).
However in case they are not directly accessible in your environment, you can also specify a custom
url to download Coursier. For example:

```
$ bazel build @maven_with_unsafe_shared_cache//... --repo_env=COURSIER_URL='https://my_secret_host.com/vXYZ/coursier.jar'
```

Please note it still requires the SHA to match.

### `artifact` helper macro

The `artifact` macro translates the artifact's `group:artifact` coordinates to
the label of the versionless target. This target is an
[alias](https://docs.bazel.build/versions/master/be/general.html#alias) that
points to the `java_import`/`aar_import` target in the `@maven` repository,
which includes the transitive dependencies specified in the top level artifact's
POM file.

For example, `@maven//:junit_junit` is equivalent to `artifact("junit:junit")`.

To use it, add the load statement to the top of your BUILD file:

```python
load("@rules_jvm_external//:defs.bzl", "artifact")
```

Full `group:artifact:[packaging:[classifier:]]version` maven coordinates are also
supported and translate to corresponding versionless target.

Note that usage of this macro makes BUILD file refactoring with tools like
`buildozer` more difficult, because the macro hides the actual target label at
the syntax level.

### `java_plugin_artifact` helper macro

The `java_plugin_artifact` macro finds a `java_plugin` target which can be used
to run an annotation procesor from a particular artifact.

For example, if you pull `com.google.auto.value:auto-value` into a
`maven_install`, you can use the `java_plugin_artifact` macro in the `plugins`
attribute of a target like `java_library`:

```python
java_library(
    name = "some_lib",
    srcs = ["SrcUsingAuto.java"],
    plugins = [
        java_plugin_artifact("com.google.auto.value:auto-value", "com.google.auto.value.processor.AutoValueProcessor"),
    ],
)
```

### Multiple `maven_install` declarations for isolated artifact version trees

If your WORKSPACE contains several projects that use different versions of the
same artifact, you can specify multiple `maven_install` declarations in the
WORKSPACE, with a unique repository name for each of them.

For example, if you want to use the JRE version of Guava for a server app, and
the Android version for an Android app, you can specify two `maven_install`
declarations:

```python
maven_install(
    name = "server_app",
    artifacts = [
        "com.google.guava:guava:27.0-jre",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "android_app",
    artifacts = [
        "com.google.guava:guava:27.0-android",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)
```

This way, `rules_jvm_external` will invoke coursier to resolve artifact versions for
both repositories independent of each other. Coursier will fail if it encounters
version conflicts that it cannot resolve. The two Guava targets can then be used
in BUILD files like so:

```python
java_binary(
    name = "my_server_app",
    srcs = ...
    deps = [
        # a versionless alias to @server_app//:com_google_guava_guava_27_0_jre
        "@server_app//:com_google_guava_guava",
    ]
)

android_binary(
    name = "my_android_app",
    srcs = ...
    deps = [
        # a versionless alias to @android_app//:com_google_guava_guava_27_0_android
        "@android_app//:com_google_guava_guava",
    ]
)
```

### Detailed dependency information specifications

Although you can always give a dependency as a Maven coordinate string,
occasionally special handling is required in the form of additional directives
to properly situate the artifact in the dependency tree. For example, a given
artifact may need to have one of its dependencies excluded to prevent a
conflict.

This situation is provided for by allowing the artifact to be specified as a map
containing all of the required information. This map can express more
information than the coordinate strings can, so internally the coordinate
strings are parsed into the artifact map with default values for the additional
items. To assist in generating the maps, you can pull in the file `specs.bzl`
alongside `defs.bzl` and import the `maven` struct, which provides several
helper functions to assist in creating these maps. An example:

```python
load("@rules_jvm_external//:defs.bzl", "artifact")
load("@rules_jvm_external//:specs.bzl", "maven")

maven_install(
    artifacts = [
        maven.artifact(
            group = "com.google.guava",
            artifact = "guava",
            version = "27.0-android",
            exclusions = [
                ...
            ]
        ),
        "junit:junit:4.12",
        ...
    ],
    repositories = [
        maven.repository(
            "https://some.private.maven.re/po",
            user = "johndoe",
            password = "example-password"
        ),
        "https://repo1.maven.org/maven2",
        ...
    ],
)
```

Note [when using `bzlmod`](docs/bzlmod.md) the syntax in `MODULE.bazel` is
different than shown above.

### Artifact exclusion

If you want to exclude an artifact from the transitive closure of a top level
artifact, specify its `group-id:artifact-id` in the `exclusions` attribute of
the `maven.artifact` helper:

```python
load("@rules_jvm_external//:specs.bzl", "maven")

maven_install(
    artifacts = [
        maven.artifact(
            group = "com.google.guava",
            artifact = "guava",
            version = "27.0-jre",
            exclusions = [
                maven.exclusion(
                    group = "org.codehaus.mojo",
                    artifact = "animal-sniffer-annotations"
                ),
                "com.google.j2objc:j2objc-annotations",
            ]
        ),
        # ...
    ],
    repositories = [
        # ...
    ],
)
```

You can specify the exclusion using either the `maven.exclusion` helper or the
`group-id:artifact-id` string directly.

You can also exclude artifacts globally using the `excluded_artifacts`
attribute in `maven_install`:


```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    excluded_artifacts = [
        "com.google.guava:guava",
    ],
)
```

### Compile-only dependencies

If you want to mark certain artifacts as compile-only dependencies, use the
`neverlink` attribute in the `maven.artifact` helper:

```python
load("@rules_jvm_external//:specs.bzl", "maven")

maven_install(
    artifacts = [
        maven.artifact("com.squareup", "javapoet", "1.11.0", neverlink = True),
    ],
    # ...
)
```

This instructs `rules_jvm_external` to mark the generated target for
`com.squareup:javapoet` with the `neverlink = True` attribute, making the
artifact available only for compilation and not at runtime.

### Test-only dependencies

If you want to mark certain artifacts as test-only dependencies, use the
`testonly` attribute in the `maven.artifact` helper:

```python
load("@rules_jvm_external//:specs.bzl", "maven")

maven_install(
    artifacts = [
        maven.artifact("junit", "junit", "4.13", testonly = True),
    ],
    # ...
)
```

This instructs `rules_jvm_external` to mark the generated target for
`junit:Junit` with the `testonly = True` attribute, making the
artifact available only for tests (e.g. `java_test`), or targets specifically
marked as `testonly = True`.

### Resolving user-specified and transitive dependency version conflicts

Use the `version_conflict_policy` attribute to decide how to resolve conflicts
between artifact versions specified in your `maven_install` rule and those
implicitly picked up as transitive dependencies.

The attribute value can be either `default` or `pinned`.

`default`: use [Coursier's default algorithm](https://get-coursier.io/docs/other-version-handling)
for version handling.

`pinned`: pin the versions of the artifacts that are explicitly specified in `maven_install`.

For example, pulling in guava transitively via google-cloud-storage resolves to
guava-26.0-android.

```python
maven_install(
    name = "pinning",
    artifacts = [
        "com.google.cloud:google-cloud-storage:1.66.0",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ]
)
```

```
$ bazel query @pinning//:all | grep guava_guava
@pinning//:com_google_guava_guava
@pinning//:com_google_guava_guava_26_0_android
```

Pulling in guava-27.0-android directly works as expected.

```python
maven_install(
    name = "pinning",
    artifacts = [
        "com.google.cloud:google-cloud-storage:1.66.0",
        "com.google.guava:guava:27.0-android",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ]
)
```

```
$ bazel query @pinning//:all | grep guava_guava
@pinning//:com_google_guava_guava
@pinning//:com_google_guava_guava_27_0_android
```

Pulling in guava-25.0-android (a lower version), resolves to guava-26.0-android. This is the default version conflict policy in action, where artifacts are resolved to the highest version.

```python
maven_install(
    name = "pinning",
    artifacts = [
        "com.google.cloud:google-cloud-storage:1.66.0",
        "com.google.guava:guava:25.0-android",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ]
)
```

```
$ bazel query @pinning//:all | grep guava_guava
@pinning//:com_google_guava_guava
@pinning//:com_google_guava_guava_26_0_android
```

Now, if we add `version_conflict_policy = "pinned"`, we should see guava-25.0-android getting pulled instead. The rest of non-specified artifacts still resolve to the highest version in the case of version conflicts.

```python
maven_install(
    name = "pinning",
    artifacts = [
        "com.google.cloud:google-cloud-storage:1.66.0",
        "com.google.guava:guava:25.0-android",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ]
    version_conflict_policy = "pinned",
)
```

```
$ bazel query @pinning//:all | grep guava_guava
@pinning//:com_google_guava_guava
@pinning//:com_google_guava_guava_25_0_android
```

There may be cases where you want the `default` pinning strategy, but
want one specific dependency to be pinned, no matter what. In these
cases, you can use the `force_version` attribute on the
`maven.artifact` helper to ensure this happens.

```starlark
maven_install(
    name = "forcing_versions",
    artifacts = [
        # Specify an ancient version of guava, and force its use. If we try to use `[23.3-jre]` as the version,
        # the resolution will fail when using `coursier`
        maven.artifact(
            artifact = "guava",
            force_version = True,
            group = "com.google.guava",
            version = "23.3-jre",
        ),
        # And something that depends on a more recent version of guava
        "xyz.rogfam:littleproxy:2.1.0",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)
```

In this case, once pinning is complete, guava `23.3-jre` will be selected.

### Overriding generated targets

When are using a WORKSPACE file you can override the generated targets for
artifacts with a target label of your choice. For instance, if you want to
provide your own definition of `@maven//:com_google_guava_guava` at
`//third_party/guava:guava`, specify the mapping in the `override_targets`
attribute:

```python
maven_install(
    name = "pinning",
    artifacts = [
        "com.google.guava:guava:27.0-jre",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    override_targets = {
        "com.google.guava:guava": "@//third_party/guava:guava",
    },
)
```

When you are using bzlmod you can override the generated target with
```
maven.override(
    name = "maven",
    coordinates = "com.google.guava:guava",
    target = "//third_party/guava:guava",
)
```

Note that the target label contains `@//`, which tells Bazel to reference the
target relative to your main workspace, instead of the `@maven` workspace.

The dependency that has been overridden is made available prefixed with
`original_`. That is, in the example above, the version of Guava that was
resolved could be accessed as `@maven//:original_com_google_guava_guava`.
The primary use case this is designed to support is to allow specific 
targets to have additional dependencies added (eg. to ensure a default 
implementation of key interfaces are available on the classpath without 
needing to modify every target)

### Proxies

As with other Bazel repository rules, the standard `http_proxy`, `https_proxy`
and `no_proxy` environment variables (and their uppercase counterparts) are
supported.

### Repository aliases

Maven artifact rules like `maven_jar` and `jvm_import_external` generate targets
labels in the form of `@group_artifact//jar`, like `@com_google_guava_guava//jar`. This
is different from the `@maven//:group_artifact` naming style used in this project.

As some Bazel projects depend on the `@group_artifact//jar` style labels, we
provide a `generate_compat_repositories` attribute in `maven_install`. If
enabled, JAR artifacts can also be referenced using the `@group_artifact//jar`
target label. For example, `@maven//:com_google_guava_guava` can also be
referenced using `@com_google_guava_guava//jar`.

The artifacts can also be referenced using the style used by
`java_import_external` as `@group_artifact//:group_artifact` or
`@group_artifact` for short.

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    generate_compat_repositories = True
)

load("@maven//:compat.bzl", "compat_repositories")
compat_repositories()
```

#### Repository remapping

If the `maven_jar` or `jvm_import_external` is not named according to `rules_jvm_external`'s
conventions, you can apply
[repository remapping](https://docs.bazel.build/versions/master/external.html#shadowing-dependencies)
from the expected name to the new name for compatibility.

For example, if an external dependency uses `@guava//jar`, and `rules_jvm_external`
generates `@com_google_guava_guava//jar`, apply the `repo_mapping` attribute to the external
repository WORKSPACE rule, like `http_archive` in this example:

```python
http_archive(
    name = "my_dep",
    repo_mapping = {
        "@guava": "@com_google_guava_guava",
    }
    # ...
)
```

With `repo_mapping`, all references to `@guava//jar` in `@my_dep`'s BUILD files will be mapped
to `@com_google_guava_guava//jar` instead.

### Hiding transitive dependencies

As a convenience, transitive dependencies are visible to your build rules.
However, this can lead to surprises when updating `maven_install`'s `artifacts`
list, since doing so may eliminate transitive dependencies from the build
graph.  To force rule authors to explicitly declare all directly referenced
artifacts, use the `strict_visibility` attribute in `maven_install`:

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    strict_visibility = True
)
```

It is also possible to change strict visibility value from default `//visibility:private`
to a value specified by `strict_visibility_value` attribute.

### Accessing transitive dependencies list

It is possible to retrieve full list of dependencies in the dependency tree, including
transitive, source, javadoc and other artifacts. `maven_artifacts` list contains full
versioned maven coordinate strings of all dependencies.

For example:
```python
load("@maven//:defs.bzl", "maven_artifacts")

load("@rules_jvm_external//:defs.bzl", "artifact")
load("@rules_jvm_external//:specs.bzl", "parse")

all_jar_coordinates = [c for c in maven_artifacts if parse.parse_maven_coordinate(c).get("packaging", "jar") == "jar"]
all_jar_targets = [artifact(c) for c in all_jar_coordinates]

java_library(
  name = "depends_on_everything",
  runtime_deps = all_jar_targets,
)
```

### Fetch and resolve timeout

The default timeout to fetch and resolve artifacts is 600 seconds.  If you need
to change this to resolve a large number of artifacts you can set the
`resolve_timeout` attribute in `maven_install`:

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    resolve_timeout = 900
)
```
### Ignoring empty jars

By default, if any fetched jar is empty (has 0 bytes) the corresponding artifact will still be included in the dependency tree.

If you would like to avoid such artifacts, and treat jars that are empty (i.e. their checksum equals the checksum of an
empty file) as if they were not found, you can set the `ignore_empty_files` attribute in `maven_install` to remove such
artifacts from coursier's output:

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    # ...
    ignore_empty_files = True
)
```

This option may be useful if you see empty source jars when `fetch_sources` is enabled.


### Duplicate artifact warning

By default you will be warned if there are duplicate artifacts in your artifact list. The `duplicate_version_warning` setting can be used to change this behavior. Use "none" to disable the warning and "error" to fail the build instead of warn.

```python
maven_install(
    artifacts = [
        # ...
    ],
    repositories = [
        # ...
    ],
    duplicate_version_warning = "error"
)
```

### Provide JVM options for artifact resolution

You can set the `JDK_JAVA_OPTIONS` environment variable to provide additional JVM options to the artifact resolver.

```python
build --repo_env=JDK_JAVA_OPTIONS=-Djavax.net.ssl.trustStore=<path-to-cacerts>
```
can be added to your .bazelrc file if you need to specify custom cacerts for artifact resolution.

### Provide JVM options for Coursier with `COURSIER_OPTS`

You can set up `COURSIER_OPTS` environment variable to provide some additional JVM options for Coursier.
This is a space-separated list of options.

Assume you'd like to override Coursier's memory settings:

```bash
COURSIER_OPTS="-Xms1g -Xmx4g"
```

### Resolving issues with nonstandard system default JDKs

Try to use OpenJDK explicitly if your machine or environment is set up to use a non-standard default implementation of the JDK and you encounter errors similar to the following:

```
java.lang.NullPointerException
	at java.base/jdk.internal.reflect.UnsafeFieldAccessorImpl.ensureObj(UnsafeFieldAccessorImpl.java:58)
	at java.base/jdk.internal.reflect.UnsafeObjectFieldAccessorImpl.get(UnsafeObjectFieldAccessorImpl.java:36)
	at java.base/java.lang.reflect.Field.get(Field.java:418)
	at org.robolectric.shadows.ShadowActivityThread$_ActivityThread_$$Reflector0.getActivities(Unknown Source)
	at org.robolectric.shadows.ShadowActivityThread.reset(ShadowActivityThread.java:277)
	at org.robolectric.Shadows.reset(Shadows.java:2499)
	at org.robolectric.android.internal.AndroidTestEnvironment.resetState(AndroidTestEnvironment.java:640)
	at org.robolectric.RobolectricTestRunner.lambda$finallyAfterTest$0(RobolectricTestRunner.java:361)
	at org.robolectric.util.PerfStatsCollector.measure(PerfStatsCollector.java:86)
	at org.robolectric.RobolectricTestRunner.finallyAfterTest(RobolectricTestRunner.java:359)
	at org.robolectric.internal.SandboxTestRunner$2.lambda$evaluate$2(SandboxTestRunner.java:296)
	at org.robolectric.internal.bytecode.Sandbox.lambda$runOnMainThread$0(Sandbox.java:99)
	at java.base/java.util.concurrent.FutureTask.run(FutureTask.java:264)
	at java.base/java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1130)
	at java.base/java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:630)
	at java.base/java.lang.Thread.run(Thread.java:830)
```

or

```
java.lang.UnsatisfiedLinkError: libstdc++.so.6: cannot open shared object file: No such file or directory
	at java.base/java.lang.ClassLoader$NativeLibrary.load0(Native Method)
	at java.base/java.lang.ClassLoader$NativeLibrary.load(ClassLoader.java:2444)
	at java.base/java.lang.ClassLoader$NativeLibrary.loadLibrary(ClassLoader.java:2500)
	at java.base/java.lang.ClassLoader.loadLibrary0(ClassLoader.java:2716)
	at java.base/java.lang.ClassLoader.loadLibrary(ClassLoader.java:2629)
	at java.base/java.lang.Runtime.load0(Runtime.java:769)
	at java.base/java.lang.System.load(System.java:1840)
	at org.conscrypt.NativeLibraryUtil.loadLibrary(NativeLibraryUtil.java:52)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke(NativeMethodAccessorImpl.java:62)
	at java.base/jdk.internal.reflect.DelegatingMethodAccessorImpl.invoke(DelegatingMethodAccessorImpl.java:43)
	at java.base/java.lang.reflect.Method.invoke(Method.java:566)
  ...
```

## Exporting and consuming artifacts from external repositories

If you're writing a library that has dependencies, you should define a constant that
lists all of the artifacts that your library requires. For example:

```python
# my_library/BUILD
# Public interface of the library
java_library(
  name = "my_interface",
  deps = [
    "@maven//:junit_junit",
    "@maven//:com_google_inject_guice",
  ],
)
```

```python
# my_library/library_deps.bzl
# All artifacts required by the library
MY_LIBRARY_ARTIFACTS = [
  "junit:junit:4.12",
  "com.google.inject:guice:4.0",
]
```

Users of your library can then load the constant in their `WORKSPACE` and add the
artifacts to their `maven_install`. For example:

```python
# user_project/WORKSPACE
load("@my_library//:library_deps.bzl", "MY_LIBRARY_ARTIFACTS")

maven_install(
  artifacts = [
        "junit:junit:4.11",
        "com.google.guava:guava:26.0-jre",
  ] + MY_LIBRARY_ARTIFACTS,
)
```

```python
# user_project/BUILD
java_library(
  name = "user_lib",
  deps = [
    "@my_library//:my_interface",
    "@maven//:junit_junit",
  ],
)
```

Any version conflicts or duplicate artifacts will resolved automatically.

## Publishing to External Repositories

In order to publish an artifact from your repo to a maven repository, you
must first create a `java_export` target. This is similar to a regular
`java_library`, but allows two additional parameters: the maven coordinates
and an optional template file to use for the `pom.xml` file.

```python
# user_project/BUILD
load("@rules_jvm_external//:defs.bzl", "java_export")

java_export(
  name = "exported_lib",
  maven_coordinates = "com.example:project:0.0.1",
  pom_template = "pom.tmpl",  # You can omit this
  srcs = glob(["*.java"]),
  deps = [
    "//user_project/utils",
    "@maven//:com_google_guava_guava",
  ],
)
```

If you wish to publish an artifact with Kotlin source code to a maven repository
you can use `kt_jvm_export`. This rule has the same arguments and generated
rules as `java_export`, but uses `kt_jvm_library` instead of `java_library`.

```python
# user_project/BUILD
load("@rules_jvm_external//:kt_defs.bzl", "kt_jvm_export")

kt_jvm_export(
  name = "exported_kt_lib",
  maven_coordinates = "com.example:project:0.0.1",
  srcs = glob(["*.kt"]),
)
```

In order to publish the artifact, use `bazel run`:

`bazel run --define "maven_repo=file://$HOME/.m2/repository" //user_project:exported_lib.publish`

Or, to publish to (eg) Sonatype's OSS repo:

```shell
MAVEN_USER=example_user MAVEN_PASSWORD=hunter2 bazel run --stamp \
  --define "maven_repo=https://oss.sonatype.org/service/local/staging/deploy/maven2" \
  --define gpg_sign=true \
  //user_project:exported_lib.publish`
```

Or, to publish to a Google Cloud Storage:

`bazel run --define "maven_repo=gs://example-bucket/repository" //user_project:exported_lib.publish`

Or, to publish to an Amazon S3 bucket:

`bazel run --define "maven_repo=s3://example-bucket/repository" //user_project:exported_lib.publish`

Or, to publish to a GCP Artifact Registry:

`bazel run --define "maven_repo=artifactregistry://us-west1-maven.pkg.dev/project/repository" //user_project:exported_lib.publish`

When using the `gpg_sign` option, the current default key will be used for
signing, and the `gpg` binary needs to be installed on the machine.

## Configuring the dependency resolver

`rules_jvm_external` supports different mechanisms for dependency resolution.
These can be selected using the `resolver` attribute of `maven_install`. The
default resolver is one backed by [coursier](https://get-coursier.io).

### Common options

All resolvers understand the following environment variables:

| Environment variable | Meaning                                                           |
|----------------------|-------------------------------------------------------------------|
| `RJE_VERBOSE`        | When set to `1` extra diagnostic logging will be sent to `stderr` |

### Configuring Coursier

The default resolver is backed by [coursier](https://get-coursier.io), which
is used in tools such as [sbt](https://www.scala-sbt.org). It supports being
used without a lock file, but cannot handle resolutions which require Maven
BOMs to be used. When using the coursier-backed resolver, the following
environment variables are honoured:

| Environment variable   | Meaning                                                                                                                                                                      |
|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `COURSIER_CREDENTIALS` | [Documented here](https://get-coursier.io/docs/other-credentials#inline) on the coursier site. If set to an absolute path, this will be used for configuring the credentials |

### Configuring Maven

A Maven-backed resolver can be used by using setting the `resolver`
attribute of `maven_install` to `maven`. This resolver requires the use of a
lock file. For bootstrapping purposes, this file may simply be an empty
file. When using the maven-backed resolver, the following environment
variables are honoured:

| Environment variable | Meaning                                                                                                                                                        |
|----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `RJE_ASSUME_PRESENT` | Prevents the resolver from checking remote repositories to see if a dependency is present, and just assumes it is                                              |
| `RJE_MAX_THREADS`    | Integer giving the maximum number of threads to use <br/>for downloads. The default value is whichever is lower: the number of processors on the machine, or 5 |
| `RJE_UNSAFE_CACHE`   | When set to `1` will use your `$HOME/.m2/repository` directory to speed up dependency resolution                                                               |

Using the unsafe cache option will use your local `$HOME/.m2/repository` as
a source for dependency resolutions, but will not include any local paths in
the generated lock file unless the `repositories` attribute contains `m2local`.

The Maven-backed resolver will use credentials stored in a `$HOME/.netrc`
file when performing dependency resolution

### Configuring Gradle

**This resolver is considered experimental**

A Gradle-backed resolver can be used by setting the `resolver`
attribute of `maven_install` to `gradle`. This resolver requires the
use of a lock file. For bootstrapping purposes, this file may simply
be an empty file.

## IPv6 support

Certain IPv4/IPv6 dual-stack environments may require flags to override the default settings for downloading dependencies, for both Bazel's native downloader and Coursier as a downloader:

Add:

* `startup --host_jvm_args=-Djava.net.preferIPv6Addresses=true` to your `.bazelrc` file for Bazel's native downloader.
* `-Djava.net.preferIPv6Addresses=true to the `COURSIER_OPTS` environment variable to provide JVM options for Coursier.

For more information, read the [official docs for IPv6 support in Bazel](https://bazel.build/docs/external#support-for-ipv6).

## Developing this project

### Verbose / debug mode

Set the `RJE_VERBOSE` environment variable to `true` to print `coursier`'s verbose
output. For example:

```
$ RJE_VERBOSE=true bazel run @maven//:pin
```

### Tests

In order to run tests, your system must have an Android SDK installed. You can install the Android SDK using [Android Studio](https://developer.android.com/studio), or through most system package managers.

```
$ bazel test //...
```

#### Installing the Android SDK on macOS

The instructions for installing the Android SDK on macOS can be hard
to find, but if you're comfortable using [HomeBrew](https://brew.sh),
the following steps will install what you need and set up the
`ANDROID_HOME` environment variable that's required in order to run
`rules_jvm_external`'s own tests.

```
brew install android-commandlinetools
export ANDROID_HOME="$(brew --prefix)/share/android-commandlinetools"
sdkmanager "build-tools;33.0.1" "cmdline-tools;latest" "ndk;21.4.7075529" "platform-tools" "platforms;android-33"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/21.4.7075529"
```

You can add the `export ANDROID_HOME` to your `.zshrc` or similar
config file.


### Generating documentation

Use [Stardoc](https://skydoc.bazel.build/docs/getting_started_stardoc.html) to
generate API documentation in the [docs](docs/) directory using
[generate_docs.sh](scripts/generate_docs.sh).
