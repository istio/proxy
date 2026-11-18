# Comparing rules_oci to a Dockerfile

Docker ships with both a container runtime and a "build system" for creating images, using `Dockerfile`.
A `Dockerfile` consists of multiple instructions and stages. Most of the time `FROM`, `COPY`, and `RUN` 
instructions which mutate the `rootfs` by adding or deleting files.

`rules_oci` cannot use Dockerfile to describe the build.
We think it's possible in theory, but such an effort would require some dedicated funding.

Most of the Dockerfile operators can be replaced with rules_oci, but it looks different.

Let's compare them to their rules_oci counterparts:

- `ADD`         -> Package the files using `tar()` or `pkg_tar()` and use `oci_image#layers`
- `ARG`         -> Not supported
- `CMD`         -> Use `oci_image#cmd`
- `COPY`        -> Not supported
- `ENTRYPOINT`  -> Use `oci_image#entrypoint`
- `ENV`         -> Use `oci_image#env`
- `EXPOSE`      -> Use `oci_image#exposed_ports`
- `FROM`        -> Use `oci_pull`
- `HEALTHCHECK` -> Not supported
- `LABEL`       -> Use `oci_image#labels`
- `MAINTAINER`  -> Not supported
- `ONBUILD`     -> Not supported
- `RUN`         -> See: https://github.com/bazel-contrib/rules_oci/issues/132
- `SHELL`       -> Use `oci_image#entrypoint` instead.
- `STOPSIGNAL`  -> Not supported
- `USER`        -> Not supported. Use the tar rule's mechanism for setting gid/uid
- `VOLUME`      -> Use `oci_image#volumes`
- `WORKDIR`     -> Use `oci_image#workdir`


References: 
- https://docs.docker.com/engine/reference/builder/#overview
- https://github.com/bazel-contrib/rules_oci/blob/main/docs/image.md
- https://github.com/bazel-contrib/rules_oci/blob/main/docs/pull.md
- https://github.com/aspect-build/bazel-lib/blob/main/docs/tar.md

## Example

Given the replacements above, with a Dockerfile that looks like this

```Dockerfile
FROM gcr.io/distroless/static-debian11@sha256:f4787e810dbc39dd59fcee319cf88e8a01181e1758dbd07c32ed4e14a9ba8904
COPY --from=0 /web-assets/ /
WORKDIR /
ENTRYPOINT ["/web-assets"]
```

1. Use `oci_pull` to pull the base image.

```starlark
oci_pull(
    name = "distroless_static",
    digest = "sha256:f4787e810dbc39dd59fcee319cf88e8a01181e1758dbd07c32ed4e14a9ba8904",
    image = "gcr.io/distroless/static-debian11",
    platforms = [
        "linux/amd64",
        "linux/arm64",
    ],
)
```

2. Replace `COPY` with `tar`.

```starlark
load("@tar.bzl", "tar")

tar(
    name = "web_assets",
    srcs = glob(["web-assets/**"]),
    compress = "gzip",
)
```

3. The resulting `BUILD` file would look like:

```starlark
load("@tar.bzl", "tar")

tar(
    name = "web_assets",
    srcs = glob(["web-assets/**"]),
    compress = "gzip",
)

oci_image(
    name = "app",
    base = "@distroless_static",
    layers = [
        ":web_assets"
    ],
    workdir = "/",
    entrypoint = ["/web-assets"]
)
```


## What about `RUN`?

Long story short, rules_oci doesn't have a replacement for it and the reason is that `RUN` requires us to depend 
on a running Container Daemon to work, and is non-hermetic.

See: https://github.com/bazel-contrib/rules_oci/issues/35

That said, instructions like `apk add xyz` and `apt-get install xyz` is supported by other rulesets.

- For `apt-get` see https://github.com/GoogleContainerTools/rules_distroless
- For `apk` see https://github.com/chainguard-dev/rules_apko
- For `deb` see https://github.com/bazel-contrib/rules_oci/tree/main/examples/deb
