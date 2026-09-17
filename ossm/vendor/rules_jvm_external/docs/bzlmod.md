# Using rules_jvm_external with bzlmod

Bzlmod is the new package manager for Bazel modules, included in Bazel 6.0.
It allows a significantly shorter setup than the `WORKSPACE` file used prior to bzlmod.

Note: this support is new as of early 2023, so expect some brokenness and missing features.
Please do file issues for missing bzlmod support.

See the `/examples/bzlmod` folder in this repository for a complete, tested example.

## Installation

Add the following to your `MODULE.bazel` file, setting the `version` to the latest one
available on https://registry.bazel.build/modules/rules_jvm_external:

```starlark
bazel_dep(name = "rules_jvm_external", version = "...")
maven = use_extension("@rules_jvm_external//:extensions.bzl", "maven")
maven.install(
    artifacts = [
        # This line is an example coordinate, you'd copy-paste your actual dependencies here
        # from your build.gradle or pom.xml file.
        "org.seleniumhq.selenium:selenium-java:4.4.0",
    ],
)

# You can split off individual artifacts to define artifact-specific options (this example sets `neverlink`).
# The `maven.install` and `maven.artifact` tags will be merged automatically.
maven.artifact(
    artifact = "javapoet",
    group = "com.squareup",
    neverlink = True,
    version = "1.11.1",
)

use_repo(maven, "maven")
```

Now you can run the `@maven//:pin` program to create a JSON lockfile of the transitive dependencies,
in a format that rules_jvm_external can use later. You'll check this file into the repository.

```sh
$ bazel run @maven//:pin
```

Ignore the instructions printed at the end of the output from this command, as they aren't updated
for bzlmod yet. See [#836](https://github.com/bazelbuild/rules_jvm_external/issues/836)

Due to [#835](https://github.com/bazelbuild/rules_jvm_external/issues/835) this creates a file with
a longer name than it should, so we rename it:

```sh
$ mv rules_jvm_external~4.5~maven~maven_install.json maven_install.json
```

Now that this file exists, we can update the `MODULE.bazel` to reflect that we pinned the
dependencies.

Add a `lock_file` attribute to the `maven.install()` call like so:

```starlark
maven.install(
    ...
    lock_file = "//:maven_install.json",
)
```

Now you'll be able to use the same `REPIN=1 bazel run @maven//:pin` operation described in the
[workspace instructions](/README.md#updating-maven_installjson) to update the dependencies.

## Extension and tag documentation

The extension and tag documentation can be found [in this document](bzlmod-api.md).

## Declaring dependencies in files

It is possible to use a
gradle [version catalog](https://docs.gradle.org/current/userguide/version_catalogs.html)
to declare dependencies. These should be declared in a `libs.versions.toml` file, and can be
imported to your bazel project by using the `from_toml` tag:

```starlark
maven.from_toml(
    libs_versions_toml = "//gradle:libs.versions.toml",
)
```

An example `libs.versions.toml` file could look like:

```toml
[versions]
junitJupiter = "5.12.2"

[libraries]
guava = { module = "com.google.guava:guava" }
guavaBom = { module = "com.google.guava:guava-bom", version = "33.4.8-jre" }
junitApi = { module = "org.junit.jupiter:junit-jupiter-api", version.ref = "junitJupiter" }
```

### Declaring BOMs from external files

This can be done by using the `bom_modules` attribute of the `from_toml` tag. This is
a list of gradle modules, matching the `module` in the `libs.versions.toml` file. We
can change our module declaration like so to correctly use the guava bom:

```starlark
maven.from_toml(
    libs_versions_toml = "//gradle:libs.versions.toml",
    bom_modules = [
        "com.google.guava:guava-bom",
    ],
)
```

## Artifact exclusion

The non-bzlmod instructions for how to configure
`exclusions` [from the README](../README.md#artifact-exclusion)
don't work as shown for bzlmod; it's not possible to "inline" them as shown (it will cause an `ERROR: in tag at
<root>/MODULE.bazel:22:14, error converting value for attribute artifacts: expected value of type 'string' for
element 9 of artifacts, but got None (NoneType)`). Split it like this instead:

```starlark
# https://github.com/grpc/grpc-java/issues/10576
maven.artifact(
    artifact = "grpc-core",
    exclusions = ["io.grpc:grpc-util"],
    group = "io.grpc",
    version = "1.58.0",  # Keep version in sync with below!
)
maven.install(
    artifacts = [
        "junit:junit:4.13.2",
        ...
```

Alternatively, you can use the mechanism outlined below to add exclusions.

## Modifying artifact declarations

Because artifacts are not always declared in the module file, `rules_jvm_external` offers
a mechanism for modifying artifacts that are declared elsewhere (eg. in an `install` or a
`from_toml` tag). This is done using the `amend_artifact` tag:

```starlark
maven.amend_artifact(
    coordinates = "io.grpc:grpc-core",
    exclusions = ["io.grpc:grpc-util"],
)
```

When matching artifacts that have been declared, only the `group:artifact` tuple is used
for matching.

## Module dependency layering

In order to allow modules to collaborate on required dependencies, the `bzlmod` extension will
collect the artifacts from all tags with the same `name` attribute together before performing a
dependency resolution. You'll know this is happening because a message will be printed to inform
you which modules are contributing to which namespace:

`The maven repository 'multiple_lock_files' has contributions from multiple bzlmod modules, and will be resolved together: ["bzlmod_lock_files", "rules_jvm_external"]`

In the root module, if this is expected and known, you can disable this warning by adding
the list of modules to the `known_contributing_modules` attribute of the `install` tag. The entry
to add will be printed for you as part of the warning. Once you set a value for `known_contributing_modules` then only those modules will be allowed to contribute dependencies.

The default name used is `maven`. Modules that are expected to be included via a `bazel_dep` should
avoid using the default name, and should always set their own (eg. `rules_jvm_external` uses
`rules_jvm_external_deps` for its own dependencies) The exception to this is where a module provides
functionality that would otherwise be obtained using a maven dependency.

Put another way, only projects that are only ever going to be used as root modules should use the
default name.

The message is printed so that should you need to understand why a particular dependency or
transitive dependency is at an unexpected version you'll have the information you need to diagnose
the problem.

When dependencies are layered in this way, you may see a warning similar to:

```
"WARNING: The following maven modules appear in multiple sub-modules with potentially different versions. Consider adding one of these to your root module to ensure consistent versions:
    com.google.guava:guava (31.1-jre, 33.2.1-jre)
```

The resolver will use the highest version artifact from the root and sub-modules. If the root version is not the highest you will see a warning during repinning similar to:

```
WARNING: For dependency 'com.google.protobuf:protobuf-java' the root @maven repo wants version 3.25.5, but got 4.27.2 from the bazel_worker_java bazel dep. Please update the version in your MODULE.bazel or set `force_version = True`.
```

You can either update the version in the root module to the highest version or set `force_version = True` in the root module to ensure that version will be the one used in the dependency resolution.

## Known issues

- Some error messages print instructions that don't apply under bzlmod,
  e.g. https://github.com/bazelbuild/rules_jvm_external/issues/827
