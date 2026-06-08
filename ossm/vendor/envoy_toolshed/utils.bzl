load("@aspect_bazel_lib//lib:jq.bzl", "jq")
load("@aspect_bazel_lib//lib:yq.bzl", "yq")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

def cat(name = "cat", flag = "target"):
    """
    Register the tool like so

    ```starlark

    cat(name = "mycat", flag = "mytarget")

    ```

    This can then be used like so:

    ```console

    $ bazel run //:mycat --//:mytarget=//path/to:target

    ```

    `name` and `flag` are optional and default to `cat` and `target`

    """

    native.genrule(
        name = "empty",
        outs = ["empty.txt"],
        cmd = """
        echo "" > $@
        """,
    )

    native.label_flag(
        name = flag,
        build_setting_default = ":empty",
    )

    native.genrule(
        name = "%s_sh" % name,
        outs = ["%s.sh" % name],
        cmd = """
        echo 'cat $${1}' > $@
        chmod +x $@
        """,
        srcs = [":%s" % flag],
    )

    sh_binary(
        name = name,
        srcs = ["%s_sh" % name],
        data = [":%s" % flag],
        args = ["$(location :%s)" % flag],
    )

def jqcat(
        name = "jqcat",
        flag = "target",
        jq_toolchain = "@jq_toolchains//:resolved_toolchain",
        jq_script = "@envoy_toolshed//:jq.sh"):
    """
    Register the tool like so

    ```starlark

    jqcat(name = "myjqcat", flag = "mytarget")

    ```

    This can then be used like so:

    ```console

    $ bazel run //:myjqcat --//:mytarget=//path/to:target

    ```

    `name` and `flag` are optional and default to `jqcat` and `target`

    Additional args are passed to `jq`.

    """

    native.genrule(
        name = "jqempty",
        outs = ["jqempty.txt"],
        cmd = """
        echo "" > $@
        """,
    )

    native.label_flag(
        name = flag,
        build_setting_default = ":jqempty",
    )

    sh_binary(
        name = name,
        srcs = [jq_script],
        data = [
            ":%s" % flag,
            jq_toolchain,
        ],
        args = ["$(location :%s)" % flag],
        env = {
            "JQ_BIN": "$(JQ_BIN)",
        },
        toolchains = [jq_toolchain],
    )

def json_merge(
        name,
        srcs = [],
        yaml_srcs = [],
        filter = None,
        args = None,
        data = [],
        visibility = None):
    """Generate JSON from JSON and YAML sources.

    By default the sources will be merged in jq `slurp` mode.

    Specify a jq `filter` to mangle the data.

    Example - places the sources into a dictionary with separate keys, but merging
    the data from one of the JSON files with the data from the YAML file:

    ```starlark
    json_merge(
        name = "myjson",
        srcs = [
            ":json_data.json",
            "@com_somewhere//:other_json_data.json",
        ],
        yaml_srcs = [
            ":yaml_data.yaml",
        ],
        filter = '''
        {first_data: .[0], rest_of_data: .[1] * .[2]}
        ''',
    )
    ```

    Args:
        name: Target name. Output will be `<name>.json`.
        srcs: List of JSON file labels to process.
        yaml_srcs: List of YAML file labels to convert to JSON and include.
        filter: Optional jq filter expression. If not provided, sources are merged.
        args: Optional list of additional jq arguments. Defaults to ["--slurp"].
        data: Additional data dependencies for jq.
        visibility: Visibility of the target.
    """
    if not srcs and not yaml_srcs:
        fail("At least one of `srcs` or `yaml_srcs` must be provided")

    yaml_json = []
    for i, yaml_src in enumerate(yaml_srcs):
        yaml_name = "%s_yaml_%s" % (name, i)
        yq(
            name = yaml_name,
            srcs = [yaml_src],
            args = ["-o=json"],
            outs = ["%s.json" % yaml_name],
        )
        yaml_json.append(yaml_name)

    all_srcs = srcs + yaml_json
    args = args or ["--slurp"]
    filter = filter or "reduce .[] as $item ({}; . * $item)"
    jq(
        name = name,
        srcs = all_srcs,
        out = "%s.json" % name,
        args = args,
        filter = filter,
        visibility = visibility,
        data = data,
    )
