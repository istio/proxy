"""
To load these rules, add this to the top of your `BUILD` file:

```starlark
load("@rules_oci//oci:defs.bzl", ...)
```
"""

load("@aspect_bazel_lib//lib:copy_file.bzl", "copy_file")
load("@aspect_bazel_lib//lib:directory_path.bzl", "directory_path")
load("@aspect_bazel_lib//lib:jq.bzl", "jq")
load("@aspect_bazel_lib//lib:utils.bzl", "propagate_common_rule_attributes", "propagate_well_known_tags")
load("@bazel_skylib//lib:types.bzl", "types")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//oci/private:image.bzl", _oci_image = "oci_image")
load("//oci/private:image_index.bzl", _oci_image_index = "oci_image_index")
load("//oci/private:load.bzl", _oci_tarball = "oci_load")
load("//oci/private:push.bzl", _oci_push = "oci_push")

oci_tarball_rule = _oci_tarball
oci_image_rule = _oci_image
oci_image_index_rule = _oci_image_index
oci_push_rule = _oci_push

def _propagate_to_internal_target(forwarded_kwargs):
    """Common rule attributes for implementation-detail targets.

    The `write_file`/`copy_file`/`directory_path` targets created by these macros are
    implementation details, so they should not inherit the full set of user-provided
    `tags`. We keep only the "well known" tags that affect execution/caching (e.g.
    `no-remote-cache`, `no-remote-exec`, `local`) so remote execution and
    Build-without-the-Bytes behavior is preserved, and force `manual` so the internal
    targets aren't matched by `:all` / `...` target patterns.
    """
    internal_kwargs = dict(forwarded_kwargs)
    tags = propagate_well_known_tags(internal_kwargs.get("tags", []))
    if "manual" not in tags:
        tags = tags + ["manual"]
    internal_kwargs["tags"] = tags
    return internal_kwargs

def _write_nl_seperated_file(name, kind, elems, forwarded_kwargs):
    label = "{}_write_{}".format(name, kind)
    write_file(
        name = label,
        out = "{}.{}.txt".format(name, kind),
        # %5Cn is the uri escaped newline
        content = [elem.replace("\n", "%5Cn") for elem in elems],
        **forwarded_kwargs
    )
    return label

def _digest(name, **kwargs):
    internal_kwargs = _propagate_to_internal_target(kwargs)

    # `oci_image_rule` and `oci_image_index_rule` produce a directory as default output.
    # Label for the [name]/index.json file
    directory_path(
        name = "{}_index_json".format(name),
        directory = name,
        path = "index.json",
        **internal_kwargs
    )

    copy_file(
        name = "{}_index_json_cp".format(name),
        src = "{}_index_json".format(name),
        out = "{}_index.json".format(name),
        **internal_kwargs
    )

    # Matches the [name].digest target produced by rules_docker container_image.
    # This is a documented, user-facing output, so it keeps the full set of tags.
    jq(
        name = name + ".digest",
        args = ["--raw-output"],
        srcs = ["{}_index.json".format(name)],
        filter = """.manifests[0].digest""",
        out = name + ".json.sha256",  # path chosen to match rules_docker for easy migration
        **kwargs
    )

def oci_image_index(name, annotations = None, **kwargs):
    """Macro wrapper around [oci_image_index_rule](#oci_image_index_rule).

    Produces a target `[name].digest`, whose default output is a file containing the sha256 digest of the resulting image.
    This is the same output as for the `oci_image` macro.

    Args:
        name: name of resulting oci_image_index_rule
        annotations: Annotations to apply to the index manifest.
            May either be specified as a file (key=value pairs, one per line) or a dict of strings to specify values inline.
        **kwargs: other named arguments to [oci_image_index_rule](#oci_image_index_rule) and
            [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).
    """
    forwarded_kwargs = propagate_common_rule_attributes(kwargs)

    if types.is_dict(annotations):
        annotations_label = "{}_write_annotations".format(name)
        write_file(
            name = annotations_label,
            out = "{}.annotations.txt".format(name),
            content = ["{}={}".format(key, value) for (key, value) in annotations.items()],
            **forwarded_kwargs
        )
        annotations = annotations_label

    oci_image_index_rule(
        name = name,
        annotations = annotations,
        **kwargs
    )

    _digest(
        name = name,
        **forwarded_kwargs
    )

def oci_image(
        name,
        created = None,
        labels = None,
        annotations = None,
        env = None,
        cmd = None,
        entrypoint = None,
        exposed_ports = None,
        volumes = None,
        **kwargs):
    """Macro wrapper around [oci_image_rule](#oci_image_rule).

    Allows labels and annotations to be provided as a dictionary, in addition to a text file.
    See https://github.com/opencontainers/image-spec/blob/main/annotations.md

    Label/annotation/env can by configured using either dict(key->value) or a file that contains key=value pairs
    (one per line). The file can be preprocessed using (e.g. using `jq`) to supply external (potentially not
    deterministic) information when running with `--stamp` flag.  See the example in
    [/examples/labels/BUILD.bazel](https://github.com/bazel-contrib/rules_oci/blob/main/examples/labels/BUILD.bazel).

    Produces a target `[name].digest`, whose default output is a file containing the sha256 digest of the resulting image.
    This is similar to the same-named target created by rules_docker's `container_image` macro.

    Args:
        name: name of resulting oci_image_rule
        created: Label to a file containing a single datetime string.
            The content of that file is used as the value of the `created` field in the image config.
        labels: Labels for the image config.
            May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.
        annotations: Annotations for the image config.
            May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.
        env: Environment variables provisioned by default to the running container.
            May either be specified as a file, as with the documentation above, or a dict of strings to specify values inline.
        cmd: Command & argument configured by default in the running container.
            May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.
        entrypoint: Entrypoint configured by default in the running container.
            May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.
        exposed_ports: Exposed ports in the running container.
            May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.
        volumes: Volumes for the container.
            May either be specified as a file, as with the documentation above. or a list of strings to specify values inline.
        **kwargs: other named arguments to [oci_image_rule](#oci_image_rule) and
            [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).
    """
    forwarded_kwargs = propagate_common_rule_attributes(kwargs)
    internal_kwargs = _propagate_to_internal_target(forwarded_kwargs)

    if types.is_dict(annotations):
        annotations_label = "{}_write_annotations".format(name)
        write_file(
            name = annotations_label,
            out = "{}.annotations.txt".format(name),
            content = ["{}={}".format(key, value) for (key, value) in annotations.items()],
            **internal_kwargs
        )
        annotations = annotations_label

    if types.is_dict(labels):
        labels_label = "{}_write_labels".format(name)
        write_file(
            name = labels_label,
            out = "{}.labels.txt".format(name),
            content = ["{}={}".format(key, value) for (key, value) in labels.items()],
            **internal_kwargs
        )
        labels = labels_label

    if types.is_dict(env):
        env_label = "{}_write_env".format(name)
        write_file(
            name = env_label,
            out = "{}.env.txt".format(name),
            content = ["{}={}".format(key, value) for (key, value) in env.items()],
            **internal_kwargs
        )
        env = env_label

    if types.is_list(cmd):
        cmd = _write_nl_seperated_file(
            name = name,
            kind = "cmd",
            elems = cmd,
            forwarded_kwargs = internal_kwargs,
        )

    if types.is_list(entrypoint):
        entrypoint = _write_nl_seperated_file(
            name = name,
            kind = "entrypoint",
            elems = entrypoint,
            forwarded_kwargs = internal_kwargs,
        )

    if types.is_list(exposed_ports):
        exposed_ports_label = "{}_write_exposed_ports".format(name)
        write_file(
            name = exposed_ports_label,
            out = "{}.exposed_ports.txt".format(name),
            content = [",".join(exposed_ports)],
            **internal_kwargs
        )
        exposed_ports = exposed_ports_label

    if types.is_list(volumes):
        volumes_label = "{}_write_volumes".format(name)
        write_file(
            name = volumes_label,
            out = "{}.volumes.txt".format(name),
            content = [",".join(volumes)],
            **internal_kwargs
        )
        volumes = volumes_label

    oci_image_rule(
        name = name,
        created = created,
        annotations = annotations,
        labels = labels,
        env = env,
        cmd = cmd,
        entrypoint = entrypoint,
        exposed_ports = exposed_ports,
        volumes = volumes,
        **kwargs
    )

    _digest(
        name = name,
        **forwarded_kwargs
    )

def oci_push(name, remote_tags = None, **kwargs):
    """Macro wrapper around [oci_push_rule](#oci_push_rule).

    Allows the remote_tags attribute to be a list of strings in addition to a text file.

    Args:
        name: name of resulting oci_push_rule
        remote_tags: a list of tags to apply to the image after pushing,
            or a label of a file containing tags one-per-line.
            See [stamped_tags](https://github.com/bazel-contrib/rules_oci/blob/main/examples/push/stamp_tags.bzl)
            as one example of a way to produce such a file.
        **kwargs: other named arguments to [oci_push_rule](#oci_push_rule) and
            [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).
    """
    internal_kwargs = _propagate_to_internal_target(propagate_common_rule_attributes(kwargs))

    if types.is_list(remote_tags):
        tags_label = "{}_write_tags".format(name)
        write_file(
            name = tags_label,
            out = "{}.tags.txt".format(name),
            content = remote_tags,
            **internal_kwargs
        )
        remote_tags = tags_label

    oci_push_rule(
        name = name,
        remote_tags = remote_tags,
        **kwargs
    )

def oci_load(name, repo_tags = None, **kwargs):
    """Macro wrapper around [oci_tarball_rule](#oci_tarball_rule).

    Allows the repo_tags attribute to be a list of strings in addition to a text file.

    Args:
        name: name of resulting oci_tarball_rule
        repo_tags: a list of repository:tag to specify when loading the image,
            or a label of a file containing tags one-per-line.
            See [stamped_tags](https://github.com/bazel-contrib/rules_oci/blob/main/examples/push/stamp_tags.bzl)
            as one example of a way to produce such a file.
        **kwargs: other named arguments to [oci_tarball_rule](#oci_tarball_rule) and
            [common rule attributes](https://bazel.build/reference/be/common-definitions#common-attributes).
    """
    internal_kwargs = _propagate_to_internal_target(propagate_common_rule_attributes(kwargs))

    if types.is_list(repo_tags):
        tags_label = "{}_write_tags".format(name)
        write_file(
            name = tags_label,
            out = "{}.tags.txt".format(name),
            content = repo_tags,
            **internal_kwargs
        )
        repo_tags = tags_label

    oci_tarball_rule(
        name = name,
        repo_tags = repo_tags,
        **kwargs
    )
