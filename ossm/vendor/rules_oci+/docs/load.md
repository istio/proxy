<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Load an oci_image into runtimes such as podman and docker.
Intended for use with `bazel run`.

For example, given an `:image` target, you could write

```
oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["my-repository:latest"],
)
```

and then run it in a container like so:

```
bazel run :load
docker run --rm my-repository:latest
```

<a id="oci_load"></a>

## oci_load

<pre>
load("@rules_oci//oci/private:load.bzl", "oci_load")

oci_load(<a href="#oci_load-name">name</a>, <a href="#oci_load-format">format</a>, <a href="#oci_load-image">image</a>, <a href="#oci_load-loader">loader</a>, <a href="#oci_load-repo_tags">repo_tags</a>)
</pre>

Loads an OCI layout into a container daemon without needing to publish the image first.

Passing anything other than oci_image to the image attribute will lead to build time errors.

### Build Outputs

The default output is an mtree specification file.
This is because producing the tarball in `bazel build` is expensive, and should typically not be an input to any other build actions,
so producing it only creates unnecessary load on the action cache.

If needed, the `tarball` output group allows you to depend on the tar output from another rule.

On the command line, `bazel build //path/to:my_tarball --output_groups=+tarball`

or in a BUILD file:

```starlark
oci_load(
    name = "my_tarball",
    ...
)
filegroup(
    name = "my_tarball.tar",
    srcs = [":my_tarball"],
    output_group = "tarball",
)
```

### When using `format = "oci"`

When using format = oci, containerd image store needs to be enabled in order for the oci style tarballs to work.

On docker desktop this can be enabled by visiting `Settings (cog icon) -> Features in development -> Use containerd for pulling and storing images`

For more information, see https://docs.docker.com/desktop/containerd/

### Multiple images

To load more than one image into the daemon,
use [rules_multirun] to group multiple oci_load targets into one executable target.

This might be useful with a docker-compose workflow, for example.

```starlark
load("@rules_multirun//:defs.bzl", "command", "multirun")

IMAGES = {
    "webservice": "//path/to/web-service:image.load",
    "backend": "//path/to/backend-service:image.load",
}

[
    command(
        name = k,
        command = v,
    )
    for (k, v) in IMAGES.items()
]

multirun(
    name = "load_all",
    commands = IMAGES.keys(),
)
```

[rules_multirun]: https://github.com/keith/rules_multirun

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="oci_load-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="oci_load-format"></a>format |  Format of image to generate. Options are: docker, oci. Currently, when the input image is an image_index, only oci is supported, and when the input image is an image, only docker is supported. Conversions between formats may be supported in the future.   | String | optional |  `"docker"`  |
| <a id="oci_load-image"></a>image |  Label of a directory containing an OCI layout, typically `oci_image`   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="oci_load-loader"></a>loader |  Alternative target for a container cli tool that will be used to load the image into the local engine when using `bazel run` on this target.<br><br>By default, we look for `docker` or `podman` or `nerdctl` on the PATH, and run the `load` command.<br><br>See the _run_template attribute for the script that calls this loader tool.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_load-repo_tags"></a>repo_tags |  a file containing repo_tags, one per line.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


