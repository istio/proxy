load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo", "string_flag")

def _string_flag_file_impl(ctx):
    flavor_value = ctx.attr.flag[BuildSettingInfo].value
    output_file = ctx.actions.declare_file(ctx.outputs.output.basename)
    ctx.actions.write(output_file, flavor_value)
    return [DefaultInfo(files=depset([output_file]))]

string_flag_file = rule(
    implementation = _string_flag_file_impl,
    attrs = {
        "flag": attr.label(),
    },
    outputs = {
        "output": "%{name}_output.txt",
    }
)

def string_flag_output(name, flag, default = ""):
    """
    Given the following BUILD usage

    ```starlark
    load("@envoy_toolshed//:flags.bzl", "string_flag_output")

    string_flag_output(
        name = "soup",
        flag = "flavor",
        default = "brocoli",
    )

    genrule(
        name = "dinner",
        outs = ["dinner.txt"],
        cmd = """
        echo "Soup: $$(cat $(location :soup))" > $@
        """,
        srcs = [":soup"],
    )

    ```

    You can specify the flavour like so:

    ```console

    $ bazel build //:dinner --//:flavor=carrot

    ```

    Which with above example will create a file containing:

    ```text
    Soup: carrot
    ```

    `default` is optional and defaults to an empty string.

    """
    string_flag(
        name = flag,
        build_setting_default = default,
    )

    string_flag_file(
        name = name,
        flag = ":%s" % flag,
    )
