# OCI images (`//oci`)

Tools for building OCI images (eg with rules_oci) on an existing base image.

## `oci_base_edit`

rules_oci can not run anything in an image, so the `RUN` steps of a Dockerfile that add users or
install packages on a base image have to be replaced. rules_distroless provides the packages
(`apt`), but its `passwd`, `group` and `dpkg_status` write the whole of those files, which is right
for an image built from nothing, but replaces the files of a base image.

`oci_base_edit` reads `/etc/{passwd,group,shadow,gshadow}` and `/var/lib/dpkg/status` from the
layers of the base image, and creates a layer with:

- the users and groups added, as `adduser` would add them (system ids from 100, the user's group
  with the same id where it is free), and their home directories;
- the packages (their control archives) added to the dpkg status as installed, so that the image
  lists them (eg for scanners).

```starlark
load("@envoy_toolshed//oci:defs.bzl", "oci_base_edit")

oci_base_edit(
    name = "base_edit",
    image = "@ubuntu_base",
    users = [{"name": "envoy", "group": True}],
    packages = [
        "@ubuntu_jammy//ca-certificates:control",
        "@ubuntu_jammy//tzdata:control",
    ],
)
```

The output only depends on the base image and the arguments: new shadow entries take the
password-change date of `root` in the base.

Not handled (yet): package maintainer scripts (eg the certificate bundle of `ca-certificates`, use
rules_distroless `cacerts`), and image config that rules_oci does not expose (eg `HEALTHCHECK`).
