"""Tools for building OCI images on a base image."""

load("//oci/private:base_edit.bzl", _base_edit = "base_edit")

def oci_base_edit(name, image, users = [], groups = [], packages = [], **kwargs):
    """Edit the system files of a base image, as package and user management tools would.

    Reads `/etc/{passwd,group,shadow,gshadow}` and `/var/lib/dpkg/status` from the layers of
    `image`, and creates a layer (`<name>.tar`) with the users and groups added, their home
    directories, and the packages added to the dpkg status. Nothing is run in the image.

    This is for adding to an existing base, eg the `RUN adduser ...` or `RUN apt-get install ...`
    steps of a Dockerfile, where tools that write the whole of these files (eg rules_distroless
    `passwd`, `group`, `dpkg_status`) would replace those of the base.

    Example, for `adduser --group --system envoy`, and packages installed with rules_distroless:

    ```starlark
    oci_base_edit(
        name = "base_edit",
        image = "@ubuntu_base",
        users = [{"name": "envoy", "group": True}],
        packages = [
            "@ubuntu_jammy//ca-certificates:control",
            "@ubuntu_jammy//tzdata:control",
        ],
    )

    oci_image(
        name = "image",
        base = "@ubuntu_base",
        tars = [
            ":base_edit",
            "@ubuntu_jammy//ca-certificates:data",
            "@ubuntu_jammy//tzdata:data",
            ...
        ],
    )
    ```

    Args:
        name: name of the target.
        image: the base image, an OCI image layout for a single platform (eg an `oci_pull` repo).
        users: users to add, dicts of:
            `name`;
            `uid` (default: the first free system id, from 100);
            `group`: `True` to add a group of the same name, with the same id if it is free
              (as `adduser --system --group`);
            `gid` or `primary_group` (default `nogroup`), if not `group`;
            `home` (default `/home/<name>`), `create_home` (default `True`),
            `home_mode` (default `0750`);
            `shell` (default `/usr/sbin/nologin`);
            `gecos`.
        groups: groups to add, dicts of `name`, and `gid` (default: the first free system id).
        packages: Debian package control archives, to add to the dpkg status as installed.
        **kwargs: common attributes.
    """
    _base_edit(
        name = name,
        image = image,
        packages = packages,
        spec = json.encode({
            "groups": groups,
            "users": users,
        }),
        **kwargs
    )
