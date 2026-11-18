<!-- Generated with Stardoc: http://skydoc.bazel.build -->

To load these rules, add this to the top of your `BUILD` file:

```starlark
load("@rules_oci//oci:defs.bzl", ...)
```

<a id="oci_image_rule"></a>

## oci_image_rule

<pre>
load("@rules_oci//oci:defs.bzl", "oci_image_rule")

oci_image_rule(<a href="#oci_image_rule-name">name</a>, <a href="#oci_image_rule-annotations">annotations</a>, <a href="#oci_image_rule-architecture">architecture</a>, <a href="#oci_image_rule-base">base</a>, <a href="#oci_image_rule-cmd">cmd</a>, <a href="#oci_image_rule-created">created</a>, <a href="#oci_image_rule-entrypoint">entrypoint</a>, <a href="#oci_image_rule-env">env</a>, <a href="#oci_image_rule-exposed_ports">exposed_ports</a>,
               <a href="#oci_image_rule-labels">labels</a>, <a href="#oci_image_rule-os">os</a>, <a href="#oci_image_rule-resource_set">resource_set</a>, <a href="#oci_image_rule-tars">tars</a>, <a href="#oci_image_rule-user">user</a>, <a href="#oci_image_rule-variant">variant</a>, <a href="#oci_image_rule-volumes">volumes</a>, <a href="#oci_image_rule-workdir">workdir</a>)
</pre>

Build an OCI compatible container image.

Note, most users should use the wrapper macro instead of this rule directly.
See [oci_image](#oci_image).

It takes number of tar files as layers to create image filesystem.
For incrementality, use more fine-grained tar files to build up the filesystem,
and choose an order so that less-frequently changed files appear earlier in the list.

```starlark
oci_image(
    # do not sort
    tars = [
        "rootfs.tar",
        "appfs.tar",
        "libc6.tar",
        "passwd.tar",
    ]
)
```

To base an oci_image on another oci_image, the `base` attribute can be used.

```starlark
oci_image(
    base = "//sys:base",
    tars = [
        "appfs.tar"
    ]
)
```

To combine `env` with environment variables from the `base`, bash style variable syntax can be used.

```starlark
oci_image(
    name = "base",
    env = {"PATH": "/usr/bin"}
)

oci_image(
    name = "app",
    base = ":base",
    env = {"PATH": "/usr/local/bin:$PATH"}
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="oci_image_rule-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="oci_image_rule-annotations"></a>annotations |  A file containing a dictionary of annotations. Each line should be in the form `name=value`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-architecture"></a>architecture |  The CPU architecture which the binaries in this image are built to run on. eg: `arm64`, `arm`, `amd64`, `s390x`. See $GOARCH documentation for possible values: https://go.dev/doc/install/source#environment   | String | optional |  `""`  |
| <a id="oci_image_rule-base"></a>base |  Label to an oci_image target to use as the base.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-cmd"></a>cmd |  A file containing a newline separated list to be used as the `command & args` of the container. These values act as defaults and may be replaced by any specified when creating a container.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-created"></a>created |  The datetime when the image was created. This can be a file containing a string in the format `YYYY-MM-DDTHH:MM:SS.sssZ` Typically, you'd provide a file containing a stamp variable replaced by the datetime of the build when executed with `--stamp`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-entrypoint"></a>entrypoint |  A file containing a newline separated list to be used as the `entrypoint` to execute when the container starts. These values act as defaults and may be replaced by an entrypoint specified when creating a container. NOTE: Setting this attribute will reset the `cmd` attribute   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-env"></a>env |  A file containing the default values for the environment variables of the container. These values act as defaults and are merged with any specified when creating a container. Entries replace the base environment variables if any of the entries has conflicting keys. To merge entries with keys specified in the base, `${KEY}` or `$KEY` syntax may be used.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-exposed_ports"></a>exposed_ports |  A file containing a comma separated list of exposed ports. (e.g. 2000/tcp, 3000/udp or 4000. No protocol defaults to tcp).   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-labels"></a>labels |  A file containing a dictionary of labels. Each line should be in the form `name=value`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-os"></a>os |  The name of the operating system which the image is built to run on. eg: `linux`, `windows`. See $GOOS documentation for possible values: https://go.dev/doc/install/source#environment   | String | optional |  `""`  |
| <a id="oci_image_rule-resource_set"></a>resource_set |  A predefined function used as the resource_set for actions.<br><br>Used with --experimental_action_resource_set to reserve more RAM/CPU, preventing Bazel overscheduling resource-intensive actions.<br><br>By default, Bazel allocates 1 CPU and 250M of RAM. https://github.com/bazelbuild/bazel/blob/058f943037e21710837eda9ca2f85b5f8538c8c5/src/main/java/com/google/devtools/build/lib/actions/AbstractAction.java#L77   | String | optional |  `"default"`  |
| <a id="oci_image_rule-tars"></a>tars |  List of tar files to add to the image as layers. Do not sort this list; the order is preserved in the resulting image. Less-frequently changed files belong in lower layers to reduce the network bandwidth required to pull and push.<br><br>The authors recommend [dive](https://github.com/wagoodman/dive) to explore the layering of the resulting image.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="oci_image_rule-user"></a>user |  The `username` or `UID` which is a platform-specific structure that allows specific control over which user the process run as. This acts as a default value to use when the value is not specified when creating a container. For Linux based systems, all of the following are valid: `user`, `uid`, `user:group`, `uid:gid`, `uid:group`, `user:gid`. If `group/gid` is not specified, the default group and supplementary groups of the given `user/uid` in `/etc/passwd` from the container are applied.   | String | optional |  `""`  |
| <a id="oci_image_rule-variant"></a>variant |  The variant of the specified CPU architecture. eg: `v6`, `v7`, `v8`. See: https://github.com/opencontainers/image-spec/blob/main/image-index.md#platform-variants for more.   | String | optional |  `""`  |
| <a id="oci_image_rule-volumes"></a>volumes |  A file containing a comma separated list of volumes. (e.g. /srv/data,/srv/other-data)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_rule-workdir"></a>workdir |  Sets the current working directory of the `entrypoint` process in the container. This value acts as a default and may be replaced by a working directory specified when creating a container.   | String | optional |  `""`  |


<a id="oci_image"></a>

## oci_image

<pre>
load("@rules_oci//oci:defs.bzl", "oci_image")

oci_image(<a href="#oci_image-name">name</a>, <a href="#oci_image-created">created</a>, <a href="#oci_image-labels">labels</a>, <a href="#oci_image-annotations">annotations</a>, <a href="#oci_image-env">env</a>, <a href="#oci_image-cmd">cmd</a>, <a href="#oci_image-entrypoint">entrypoint</a>, <a href="#oci_image-exposed_ports">exposed_ports</a>, <a href="#oci_image-volumes">volumes</a>,
          <a href="#oci_image-kwargs">**kwargs</a>)
</pre>

Macro wrapper around [oci_image_rule](#oci_image_rule).

Allows labels and annotations to be provided as a dictionary, in addition to a text file.
See https://github.com/opencontainers/image-spec/blob/main/annotations.md

Label/annotation/env can by configured using either dict(key->value) or a file that contains key=value pairs
(one per line). The file can be preprocessed using (e.g. using `jq`) to supply external (potentially not
deterministic) information when running with `--stamp` flag.  See the example in
[/examples/labels/BUILD.bazel](https://github.com/bazel-contrib/rules_oci/blob/main/examples/labels/BUILD.bazel).

Produces a target `[name].digest`, whose default output is a file containing the sha256 digest of the resulting image.
This is similar to the same-named target created by rules_docker's `container_image` macro.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="oci_image-name"></a>name |  name of resulting oci_image_rule   |  none |
| <a id="oci_image-created"></a>created |  Label to a file containing a single datetime string. The content of that file is used as the value of the `created` field in the image config.   |  `None` |
| <a id="oci_image-labels"></a>labels |  Labels for the image config. May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.   |  `None` |
| <a id="oci_image-annotations"></a>annotations |  Annotations for the image config. May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.   |  `None` |
| <a id="oci_image-env"></a>env |  Environment variables provisioned by default to the running container. May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.   |  `None` |
| <a id="oci_image-cmd"></a>cmd |  Command & argument configured by default in the running container. May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.   |  `None` |
| <a id="oci_image-entrypoint"></a>entrypoint |  Entrypoint configured by default in the running container. May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.   |  `None` |
| <a id="oci_image-exposed_ports"></a>exposed_ports |  Exposed ports in the running container. May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.   |  `None` |
| <a id="oci_image-volumes"></a>volumes |  Volumes for the container. May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.   |  `None` |
| <a id="oci_image-kwargs"></a>kwargs |  other named arguments to [oci_image_rule](#oci_image_rule) and [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).   |  none |


