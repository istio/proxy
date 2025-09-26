
def unpacker(
        name,
        script = "@envoy_toolshed//tarball:unpack.sh",
        zstd = None,
        visibility = ["//visibility:public"],
):

    native.genrule(
        name = "placeholder",
        outs = ["PLACEHOLDER.TXT"],
        cmd = """
        touch $@
        """,
    )

    native.filegroup(
        name = "true",
        srcs = [],
    )

    native.filegroup(
        name = "false",
        srcs = [],
    )

    native.filegroup(
        name = "empty",
        srcs = [":placeholder"],
    )

    native.label_flag(
        name = "target",
        build_setting_default = ":empty",
        visibility = ["//visibility:public"],
    )

    native.label_flag(
        name = "overwrite",
        build_setting_default = ":false",
        visibility = ["//visibility:public"],
    )

    native.config_setting(
        name = "overwrite_enabled",
        flag_values = {":overwrite": ":true"},
    )

    env = {"TARGET": "$(location :target)"}
    data = [":target"]
    if zstd:
        data += [zstd]
        env["ZSTD"] = "$(location %s)" % zstd

    native.sh_binary(
        name = name,
        srcs = [script],
        visibility = visibility,
        data = data,
        env = env | select({
            ":overwrite_enabled": {"OVERWRITE": "1"},
            "//conditions:default": {}}),
    )
