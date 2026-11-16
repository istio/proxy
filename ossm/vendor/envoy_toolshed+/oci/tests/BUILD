load("@rules_python//python:defs.bzl", "py_test")
load("//oci:defs.bzl", "oci_base_edit")

# A three layer image layout (with whiteouts, see `base_edit_test.py`), and package control archives.
filegroup(
    name = "layout",
    srcs = glob(["layout/**"]),
)

oci_base_edit(
    name = "base_edit",
    groups = [{
        "name": "proxy",
        "gid": 200,
    }],
    image = ":layout",
    packages = [
        "ca-certificates.control.tar.gz",
        "tzdata.control.tar.gz",
    ],
    users = [
        {
            # As `adduser --group --system envoy`.
            "name": "envoy",
            "group": True,
        },
        {
            "name": "worker",
            "create_home": False,
            "primary_group": "users",
        },
    ],
)

py_test(
    name = "base_edit_test",
    srcs = ["base_edit_test.py"],
    args = ["$(rootpath :base_edit)"],
    data = [":base_edit"],
)
