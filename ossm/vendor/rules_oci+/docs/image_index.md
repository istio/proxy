<!-- Generated with Stardoc: http://skydoc.bazel.build -->

To load these rules, add this to the top of your `BUILD` file:

```starlark
load("@rules_oci//oci:defs.bzl", ...)
```

<a id="oci_image_index_rule"></a>

## oci_image_index_rule

<pre>
load("@rules_oci//oci:defs.bzl", "oci_image_index_rule")

oci_image_index_rule(<a href="#oci_image_index_rule-name">name</a>, <a href="#oci_image_index_rule-annotations">annotations</a>, <a href="#oci_image_index_rule-images">images</a>, <a href="#oci_image_index_rule-platforms">platforms</a>)
</pre>

Build a multi-architecture OCI compatible container image.

It takes number of `oci_image` targets to create a fat multi-architecture image conforming to [OCI Image Index Specification](https://github.com/opencontainers/image-spec/blob/main/image-index.md).

Image indexes can be created in two ways:

## Using Bazel platforms

While this feature is still experimental, it is the recommended way to create image indexes.

```starlark
go_binary(
    name = "app_can_cross_compile"
)

tar(
    name = "app_layer",
    srcs = [
        ":app_can_cross_compile",
    ],
)

oci_image(
    name = "image",
    tars = [":app_layer"],
)

oci_image_index(
    name = "image_multiarch",
    images = [":image"],
    platforms = [
        "@rules_go//go/toolchain:linux_amd64",
        "@rules_go//go/toolchain:linux_arm64",
    ],
)
```

## Without using Bazel platforms

```starlark
oci_image(
    name = "app_linux_amd64"
)

oci_image(
    name = "app_linux_arm64"
)

oci_image_index(
    name = "app",
    images = [
        ":app_linux_amd64",
        ":app_linux_arm64"
    ]
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="oci_image_index_rule-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="oci_image_index_rule-annotations"></a>annotations |  A file containing a dictionary of annotations to apply to the index manifest. Each line should be in the form `name=value`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="oci_image_index_rule-images"></a>images |  List of labels to oci_image targets.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="oci_image_index_rule-platforms"></a>platforms |  This feature is highly EXPERIMENTAL and not subject to our usual SemVer guarantees. A list of platform targets to build the image for. If specified, only one image can be specified in the images attribute.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="oci_image_index"></a>

## oci_image_index

<pre>
load("@rules_oci//oci:defs.bzl", "oci_image_index")

oci_image_index(<a href="#oci_image_index-name">name</a>, <a href="#oci_image_index-annotations">annotations</a>, <a href="#oci_image_index-kwargs">**kwargs</a>)
</pre>

Macro wrapper around [oci_image_index_rule](#oci_image_index_rule).

Produces a target `[name].digest`, whose default output is a file containing the sha256 digest of the resulting image.
This is the same output as for the `oci_image` macro.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="oci_image_index-name"></a>name |  name of resulting oci_image_index_rule   |  none |
| <a id="oci_image_index-annotations"></a>annotations |  Annotations to apply to the index manifest. May either be specified as a file (key=value pairs, one per line) or a dict of strings to specify values inline.   |  `None` |
| <a id="oci_image_index-kwargs"></a>kwargs |  other named arguments to [oci_image_index_rule](#oci_image_index_rule) and [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).   |  none |


