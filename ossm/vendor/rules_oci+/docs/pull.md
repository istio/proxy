<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A repository rule to pull image layers using Bazel's downloader.

Typical usage in `WORKSPACE.bazel`:

```starlark
load("@rules_oci//oci:pull.bzl", "oci_pull")

# A single-arch base image
oci_pull(
    name = "distroless_java",
    digest = "sha256:161a1d97d592b3f1919801578c3a47c8e932071168a96267698f4b669c24c76d",
    image = "gcr.io/distroless/java17",
    platforms = ["linux/amd64"],  # Optional
)

# A multi-arch base image
oci_pull(
    name = "distroless_static",
    digest = "sha256:c3c3d0230d487c0ad3a0d87ad03ee02ea2ff0b3dcce91ca06a1019e07de05f12",
    image = "gcr.io/distroless/static",
    platforms = [
        "linux/amd64",
        "linux/arm64",
    ],
)

# A multi-arch base image with variants, note that it won't work with just "linux/arm64"
oci_pull(
    name = "distroless_base_nossl_debian12",
    digest = "sha256:73c3d3f3030516665c916ebc9baa80f89c1a90e438dc02f1fed525ed246c0c2a",
    image = "gcr.io/distroless/base-nossl-debian12",
    platforms = [
        "linux/amd64",
        "linux/arm64/v8",
    ],
)
```

Typical usage in `MODULE.bazel`:

```starlark
oci = use_extension("@rules_oci//oci:extensions.bzl", "oci")

# A multi-arch base image with variants, note that it won't work with just "linux/arm64"
oci.pull(
    name = "distroless_base_nossl_debian12",
    digest = "sha256:73c3d3f3030516665c916ebc9baa80f89c1a90e438dc02f1fed525ed246c0c2a",
    image = "gcr.io/distroless/base-nossl-debian12",
    platforms = [
        "linux/amd64",
        "linux/arm64/v8",
    ],
)
use_repo(
    oci,
    "distroless_base_nossl_debian12",
    "distroless_base_nossl_debian12_linux_amd64",
    "distroless_base_nossl_debian12_linux_arm64_v8",
)
```

Now you can refer to these as a base layer in `BUILD.bazel`.
The target is named the same as the external repo, so you can use a short label syntax:

```starlark
oci_image(
    name = "app",
    base = "@distroless_static",
    ...
)

oci_image(
    name = "app_arm64_v8",
    base = "@distroless_base_nossl_debian12_linux_arm64_v8",
    ...
)
```

## Configuration

Docker specifies a standard location where registry credentials are stored:
<https://docs.docker.com/engine/reference/commandline/cli/#configuration-files>

We search for this file in several locations, following the logic at
https://github.com/google/go-containerregistry/tree/main/pkg/authn#tldr-for-consumers-of-this-package.

Set `--repo_env=DOCKER_CONFIG=/some/other/directory` to cause `oci_pull` to look for
`config.json` in this directory instead.

Finally, some less-common use cases are afforded with environment variables `XDG_RUNTIME_DIR` and `REGISTRY_AUTH_FILE`.
See the implementation of `_get_auth_file_path` in `/oci/private/auth_config_locator.bzl` for the complete reference.


## Authentication using credential helpers

By default oci_pull try to mimic `docker pull` authentication mechanism to allow users simply use `docker login` for authentication.

However, this doesn't always work due to some limitations of Bazel where response headers can't be read, which prevents us from
performing `WWW-Authenticate` challenges, as we don't know which endpoint to hit to complete the challenge. To workaround this
we keep a map of known registries that require us to perform www-auth challenge to acquire a temporary token for authentication.


Fortunately, Bazel supports running external programs to authenticate http requests using the `--credential_helper` command line flag.
When the credential helper flag passed, Bazel will call the external program before sending the request to allow additional headers to be set.

An example of this

.bazelrc
```
common --credential_helper=public.ecr.aws=%workspace%/tools/auth.sh
```

tools/auth.sh
```bash
input=$(cat)
uri=$(jq -r ".uri" <<< $input)
host="$(echo $uri | awk -F[/:] '{print $4}')"
curl -fsSL https://$host/token | jq '{headers:{"Authorization": [("Bearer " + .token)]}}'
```

This tells bazel to run `%workspace%/tools/auth.sh` for any request sent to `public.ecr.aws` and add additional headers that may have been
printed to `stdout` by the external program.

For more information about the credential helpers checkout the [documentation](https://github.com/bazelbuild/proposals/blob/main/designs/2022-06-07-bazel-credential-helpers.md).

See the [examples/credential_helper](/examples/credential_helper/auth.sh) directory for an example of how this work.

When using a credential helper, it may be desirable to allow the built-in credential handling to fail. This can be achieved by setting the `$OCI_GET_TOKEN_ALLOW_FAIL` repo env:

.bazelrc
```
common --repo_env=OCI_GET_TOKEN_ALLOW_FAIL=1
```

### Providing WWW-Authenticate challenges

Due to limitations of Bazel downloader, repository rules can not look up HTTP Response headers to perform `WWW-Authenticate` challenges.
To workaround this, we keep a map of [known registries](/oci/private/authn.bzl#L9) that require us to perform www-auth challenge to acquire a temporary token for authentication.

While this works well for known registries, it does not work for private registries that require authentication. To accomodate this, `oci.pull` allows additional www-authenticate
challenges to be provided via the `www_authenticate_challenges` attribute. This is useful for private registries that require additional authentication.

```starlark
oci.pull(
    name = "my_private_image",
    image = "my.private.registry/my/image",
    digest = "sha256:deadbeef",
    www_authenticate_challenges = {
        # https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/WWW-Authenticate
        "my.private.registry": 'Bearer realm="https://my.private.registry/token",service="my.private.registry"'}
    },
)
```

In this example, we provide a `www_authenticate_challenges` attribute to the `oci.pull` rule to allow the rule to perform the `WWW-Authenticate` challenge for the `my.private.registry` registry.

<a id="oci_pull"></a>

## oci_pull

<pre>
load("@rules_oci//oci:pull.bzl", "oci_pull")

oci_pull(<a href="#oci_pull-name">name</a>, <a href="#oci_pull-www_authenticate_challenges">www_authenticate_challenges</a>, <a href="#oci_pull-image">image</a>, <a href="#oci_pull-repository">repository</a>, <a href="#oci_pull-registry">registry</a>, <a href="#oci_pull-platforms">platforms</a>, <a href="#oci_pull-digest">digest</a>, <a href="#oci_pull-tag">tag</a>,
         <a href="#oci_pull-reproducible">reproducible</a>, <a href="#oci_pull-is_bzlmod">is_bzlmod</a>, <a href="#oci_pull-config">config</a>, <a href="#oci_pull-bazel_tags">bazel_tags</a>)
</pre>

Repository macro to fetch image manifest data from a remote docker registry.

To use the resulting image, you can use the `@wkspc` shorthand label, for example
if `name = "distroless_base"`, then you can just use `base = "@distroless_base"`
in rules like `oci_image`.

> This shorthand syntax is broken on the command-line prior to Bazel 6.2.
> See https://github.com/bazelbuild/bazel/issues/4385


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="oci_pull-name"></a>name |  repository with this name is created   |  none |
| <a id="oci_pull-www_authenticate_challenges"></a>www_authenticate_challenges |  a dictionary of registry hostnames to WWW-Authenticate challenges. This is used to perform authentication challenges for private registries. See the [documentation](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/WWW-Authenticate) for more information about the WWW-Authenticate header.   |  `None` |
| <a id="oci_pull-image"></a>image |  the remote image, such as `gcr.io/bazel-public/bazel`. A tag can be suffixed with a colon, like `debian:latest`, and a digest can be suffixed with an at-sign, like `debian@sha256:e822570981e13a6ef1efcf31870726fbd62e72d9abfdcf405a9d8f566e8d7028`.<br><br>Exactly one of image or {registry,repository} should be set.   |  `None` |
| <a id="oci_pull-repository"></a>repository |  the image path beneath the registry, such as `distroless/static`. When set, registry must be set as well.   |  `None` |
| <a id="oci_pull-registry"></a>registry |  the remote registry domain, such as `gcr.io` or `docker.io`. When set, repository must be set as well.   |  `None` |
| <a id="oci_pull-platforms"></a>platforms |  a list of the platforms the image supports. Mandatory for multi-architecture images. Optional for single-architecture images, which expect a one-element list. This creates a separate external repository for each platform, avoiding fetching layers, and an alias that validates the presence of an image matching the target platform's cpu.   |  `None` |
| <a id="oci_pull-digest"></a>digest |  the digest string, starting with "sha256:", "sha512:", etc. If omitted, instructions for pinning are provided.   |  `None` |
| <a id="oci_pull-tag"></a>tag |  a tag to choose an image from the registry. Exactly one of `tag` and `digest` must be set. Since tags are mutable, this is not reproducible, so a warning is printed.   |  `None` |
| <a id="oci_pull-reproducible"></a>reproducible |  Set to False to silence the warning about reproducibility when using `tag`.   |  `True` |
| <a id="oci_pull-is_bzlmod"></a>is_bzlmod |  whether the oci_pull is being called from a module extension   |  `False` |
| <a id="oci_pull-config"></a>config |  Label to a `.docker/config.json` file.   |  `None` |
| <a id="oci_pull-bazel_tags"></a>bazel_tags |  Bazel tags to be propagated to generated rules.   |  `[]` |


