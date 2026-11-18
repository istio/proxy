---
title: Migrating to rules_oci
sidebar_label: Migrating to rules_oci
description: How to migrate from rules_docker
---

This document contains some of the lessons we've learned at Aspect from doing consulting work, migrating some large client repos from rules_docker to rules_oci.

This guide remains a work-in-progress as we find new patterns.

## Update WORKSPACE

The `WORKSPACE` file contains Bazel module dependency fetching and installation.

Add install steps from a release of rules_oci, along with related rulesets you plan to use.

Note that the `container_test` rule from rules_docker is now in a separate repo.
See [container_structure_test]

## Replacements by rule

| rules_docker          | rules_oci   | How to migrate                          |
| --------------------- | :---------- | --------------------------------------- |
| container_push        | [oci_push]  | See [below](#container_push)            |
| container_image       | [oci_image] | See [below](#container_image)           |
| container_bundle      | [oci_load]  | See [below](#container_image)           |
| container_pull        | [oci_pull]  | See [below](#container_pull)            |
| container_test        | N/A         | Use [container_structure_test]          |
| container_import      | N/A         |                                         |
| container_load        | N/A         |                                         |
| container_run_and\_\* | N/A         | See [below](#docker-run-rules)          |
| go_image              | N/A         | Wrap `go_binary`, see [go docs]         |
| java_image            | N/A         | Wrap `java_binary`, see [java docs]     |
| py3_image             | N/A         | Wrap `py_binary`, see [python docs]     |
| nodejs_image          | N/A         | Wrap `js_binary`, see [javascript docs] |
| rust_image            | N/A         | Wrap `rust_binary`, see [rust docs]     |

[container_structure_test]: https://github.com/GoogleContainerTools/container-structure-test#running-structure-tests-through-bazel
[oci_pull]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/pull.md
[oci_push]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/push.md
[oci_image]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/image.md
[oci_load]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/load.md
[go docs]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/go.md
[python docs]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/python.md
[java docs]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/java.md
[rust docs]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/rust.md
[javascript docs]: https://github.com/bazel-contrib/rules_oci/blob/main/docs/javascript.md

### container_pull

`puller_darwin`, `puller_linux_*` are unsupported. Unlike container_pull, oci_pull uses Bazel's downloader instead of a custom puller binary.

`import_tags` is unsupported. no plans to add support for it.

`cred_helpers` is unsupported. credential helpers should be installed on the host where bazel runs.

`docker_client_config` is unsupported. instead use `DOCKER_CONFIG` environment variable to override the config.

`os_version` is unsupported.

`os_features` is unsupported.

`platform_features` is unsupported.

`os`, `architecture`, `cpu_variant` is supported, however, are combined into single string that can be passed to `platforms` attributes.

```diff
-load("@io_bazel_rules_docker//container:container.bzl", "container_pull")
+load("@rules_oci//oci:pull.bzl", "oci_pull")
-container_pull(
+oci_pull(
- os = "linux",
- architecture = "arm64"
- cpu_variant = "v8"
+ platforms = [
+     "linux/arm64/v8"
+ ]
)
```

`timeout` is unsupported. Bazel's [--http_timeout_scaling](https://bazel.build/reference/command-line-reference#flag--http_timeout_scaling) can be used to increase/decrease the default timeout.

#### Environment variables

`DOCKER_REPO_CACHE` is not supported. Since oci_pull uses Bazel's downloader, remote manifests/blobs are cached via Bazel's repository cache as long as a `digest` is provided.

`PULLER_TIMEOUT` is not supported.

### container_push

Update BUILD files with:
`buildozer 'set kind oci_push' //...:%container_push`

`registry` and `repository` are merged into one attribute; `repository`.

```diff
-registry = "registry.io",
-repository = "org/image"
+repository = "registry.io/org/image"
```

`tag` attribute is now `remote_tags` and allows multiple tags.

```diff
-tag = "latest",
+remote_tags = ["latest"]
```

`insecure_repository` is not needed anymore as it's automatically detected for most cases. However, in case it can't be detected `--insecure` can be added to the `args` attribute.

```diff
-insecure_repository = true,
+args = ["--insecure"]
```

`tag_file` is now `remote_tags` and expects one tag per line.

```diff
-tag_file = ":tag_file",
+remote_tags = ":tag_file",
```

`repository_file` is now `repository` and accepts a file

```diff
-repository_file = ":repository_file",
+repository = ":repository_file",
```

`stamp` is not supported directly. We encourage users to stamp in a separate target and pass the stamped file to `repository` or `remote_tags` attribute respectively.
Note that `container_push` has different behavior where changes to `volatile-status.txt` are always seen by the program under `bazel run`, but `oci_push` does not. See [this GitHub issue](https://github.com/bazel-contrib/rules_oci/issues/269#issuecomment-1611953197).

```python
# an example from rules_oci stamping tags
stamp_tags(
    name = "stamped",
    remote_tags = [
        # With --stamp, use the --embed_label value, otherwise use 0.0.0
        """($stamp.BUILD_EMBED_LABEL // "0.0.0")""",
        "nightly",
    ],
)

oci_push(
    name = "push",
    image = ":oci_image_target",
    repository = "index.docker.io/<ORG>/image",
    remote_tags = ":stamped",
)
```

`format` is unsupported. the default format is `oci` and legacy `docker` format isn't supported.

`skip_unchanged_digest` is unsupported. `oci_push` skips existing blobs by default.

`extension` is unsupported. the default extension is `sh`. no plans to support it.

`tag_tpl` and `windows_paths` attributes are unsupported. no plans to support it.

### container_image

Update BUILD files with:
`buildozer 'set kind oci_image' //...:%container_image`

`oci_image` does not support following attributes directly, but they can be passed to `pkg_tar` for layer creation.

- `files` -> `pkg_tar#srcs`
- `compression` -> `pkg_tar#compressor`
- `compression_options` -> `pkg_tar#compressor_args`
- `data_path` -> `pkg_tar#strip_prefix`
- `directory` -> `pkg_tar#package_dir`
- `empty_dirs` -> `pkg_tar#empty_dirs`
- `empty_files` -> `pkg_tar#empty_files`
- `tars` -> `pkg_tar#deps`
- `mode` -> `pkg_tar#mode`
- `mtime` -> `pkg_tar#mtime`
- `symlinks` -> `pkg_tar#symlinks`

An example demonstrating migrating `tars`, `directory`, `symlinks` from `container_image` to `oci_image`

```diff
@@ -1,6 +1,13 @@
-container_image(
+oci_image(
     name = "image",
+    tars = [
+        ":new_layer"
+    ]
+)
+
+pkg_tar(
+    name = "new_layer",
     symlinks = { "/usr/bin/app": "/app"},
     tars = [":app"],
-    directory = "/org/image"
+    package_dir = "/org/image"
 )
```

`debs` is not supported directly as an attribute. However, since `.deb` files consist of two tar files `data.tar.xz` and `control.tar.xz`, these can be passed to `oci_image#tars` attribute after extracted from a `deb` file.

If the debian files are downloaded via `http_archive`, bazel recently landed [support](https://github.com/bazelbuild/bazel/pull/15132) for extracting these files on the fly.

An example genrule extracting `.deb` files before passing down to `oci_image`

```starlark
genrule(
    name = "new_layer",
    srcs = [":deb"],
    outs = ["data.tar.xz"],
    cmd = "tar -xvf -c $@/ $(location :deb)",
)

oci_image(
    tars = [":new_layer"]
)
```

`enable_mtime_preservation` is unsupported. pkg_tar [doesn't support](https://github.com/bazelbuild/rules_pkg/issues/265) this feature either. Generally, this is a bad idea since it leads to non-reproducible builds.

`layers` is now `tars`. it accepts list of arbitrary `.tar` or `.tar.gz` files.

Note that `oci_image#tars` attribute behaves differently than `container_image#tars`. While `container_image#tars` squashes multiple tars into single tar, `oci_image#tars` preserves tars and creates a layer per tar respecting the order of the tars given at `oci_image#tars` attribute.

```diff
-container_image(
+oci_image(
     name = "image",
-    layers = [
+    tars = [
         ":layer"
     ]
 )
```

`launcher` and `launcher_args` are unsupported. As a replacement `cmd` and `entrypoint` can be used instead. However, since `entrypoint` attribute overrides the entrypoint, it wouldn't be possible to inherit the `entrypoint` from the `base`.

```diff
-container_image(
-container_image(
+pkg_tar(
+    name = "launcher",
+    srcs = [":launcher"]
+)
+oci_image(
     name = "image",
-    launcher = ":launcher",
-    launcher_args = ["--arg1", "--arg2"],
-    entrypoint = ["/app"],
-    cmd = ["--apparg1"]
+    entrypoint = ["/path/to/launcher", "--arg1", "--arg2"]
+    cmd = ["/app", "--apparg1"],
+    tars = [
+        ":launcher"
+    ]
 )
```

`legacy_run_behavior`, `legacy_repository_naming`, and `docker_run_flags` attributes are unsupported. Instead [oci_load](https://github.com/bazel-contrib/rules_oci/blob/main/docs/load.md) rule should be used for loading the `oci_image` into a docker daemon.

> the oci_image target can be loaded into the daemon by running `bazel build :tarball`, `docker load -i bazel-bin/tarball/tarball.tar` respectively.

> `docker_run_flags` can be passed to docker directly when running `docker run gcr.io/test:latest`

```diff
-container_image(
+oci_image(
     name = "image",
+)
+
+oci_load(
+    name = "tarball",
+    image = ":image",
+    repo_tags = ["gcr.io/test:latest"]
 )
```

`repository` and `tag_name` is unsupported. use [oci_load#repo_tags](https://github.com/bazel-contrib/rules_oci/blob/main/docs/load.md#oci_load-repo_tags) as a replacement.

```diff
-container_image(
+oci_image(
     name = "image",
     tag_name = "latest",
-    repository = "gcr.io"
+    repository = "gcr.io/image"
+)
+
+oci_load(
+    name = "tarball",
+    repo_tags = ["gcr.io/image:latest"]
 )
```

`compression` and `compression_options` are unsupported. use [pkg_tar#compressor](https://github.com/bazelbuild/rules_pkg/blob/main/docs/0.8.0/reference.md#pkg_tar-compressor) and [pkg_tar#compressor_args](https://github.com/bazelbuild/rules_pkg/blob/main/docs/0.8.0/reference.md#pkg_tar-compressor_args) when creating layers instead.

`experimental_tarball_format` is unsupported. `oci_image` does not produce tarballs. [oci_load](https://github.com/bazel-contrib/rules_oci/blob/main/docs/load.md), which produces tarballs out of `oci_image`, should be used instead.

`stamp` is not supported directly. oci_image allows attributes such as `labels`, `annotations` to be stamped. If the intent is to stamp layers, [pkg_tar#stamp](https://github.com/bazelbuild/rules_pkg/blob/main/docs/0.8.0/reference.md#pkg_tar-stamp) is the preferred way to stamp layers.

`creation_time` is unsupported at the moment. by default creation time for the image is static. follow [rules_oci#49](https://github.com/bazel-contrib/rules_oci/issues/49) for updates.

`os_version` is unsupported at the moment. follow [rules_oci#48](https://github.com/bazel-contrib/rules_oci/issues/48) for updates.

`ports` is unsupported. most runtimes support `--port|-p` flag which can be passed at container startup. follow [rules_oci#220](https://github.com/bazel-contrib/rules_oci/issues/220) for updates.

`volumes` is unsupported. most runtimes support `--volume|-v|--mount` flag which can be passed at container startup.

### Docker run rules

rules_docker has [three rules](https://github.com/bazelbuild/rules_docker/blob/master/docker/util/README.md)
that rely on running a command in a Docker container during a `bazel build` action:

- `container_run_and_commit`
- `container_run_and_commit_layer`
- `container_run_and_extract`

As well as the [`dockerfile_build` rule](https://github.com/bazelbuild/rules_docker/blob/master/contrib/dockerfile_build.bzl)

However these are not hermetic, so they interact incorrectly with Bazel caching.
rules_oci does not contain an equivalent rule, see discussion on
https://github.com/bazel-contrib/rules_oci/issues/132

These are the approaches typically used instead of these rules:

1. Build a base layer, push/pull it from a registry
2. Fetch packages "the Bazel way"
3. Bazel's `docker` spawn strategy

#### 1. Build base layer

This is a two-phase strategy, and the most commonly used.

First, build a base layer containing all the system packages you need to be in the image.
It's not necessary to use Bazel for this step - many users continue using a non-hermetic `Dockerfile` in some external image-builder pipeline.
When needed, re-build that base layer and push it to some Docker registry.

Second, use `oci_pull` to fetch that base layer, then add layers on top with your application dependencies and application code.

#### 2. Fetch packages

Bazel can construct a hermetic, reproducible package given explicit instructions on fetching and installing packages, doing so outside of any containerization.
This is possible because Docker container layers are simply tar files, and most packages are trivial to install.

For this method, just use `oci_pull` to fetch a base image, then add layers containing the system packages you need, using one of these:

- For Alpine packages, see [rules_apko](https://github.com/chainguard-dev/rules_apko)
- For Debian packages, see [rules_debian_packages](https://github.com/bazel-contrib/rules_debian_packages)
- Some useful Linux-specific recipes may be found in https://github.com/GoogleContainerTools/rules_distroless

Finally, add your application's dependencies and application code as layers to the image.

#### 3. Docker spawn strategy

Bazel has long had a built-in way to run `bazel build` actions inside a container.
Some documentation for this may be found on https://bazel.build/remote/sandbox#troubleshooting-natively

In practice, Aspect doesn't know of anyone using this approach to replace rules_docker, so it may be something of a science project.
Please report back to us if you've gotten this working, so we can share your recipe with others!
