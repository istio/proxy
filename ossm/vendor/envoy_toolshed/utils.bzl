
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
        """
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

    native.sh_binary(
        name = name,
        srcs = ["%s_sh" % name],
        data = [":%s" % flag],
        args = ["$(location :%s)" % flag]
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
        """
    )

    native.label_flag(
        name = flag,
        build_setting_default = ":jqempty",
    )

    native.sh_binary(
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
