

def envoy_example(name, shared = ":shared_files", common_fun = ":verify-common.sh"):

    native.filegroup(
        name = "%s_files" % name,
        srcs = native.glob(["%s/**/*" % name]),
    )

    native.genrule(
        name = "%s_dir" % name,
        outs = ["%s_dir.tar" % name],
        cmd = """
        SHARED_PATHS="$(locations %s)"
        SHARED=$$(echo $$SHARED_PATHS | cut -d/ -f1)
        # This is a bit hacky and may not work in all bazel situations, but works for now
        if [[ $$SHARED == "external" ]]; then
            SHARED=$$(echo $$SHARED_PATHS | cut -d/ -f-3)
        fi
        EXAMPLE_PATHS="$(locations %s_files)"
        EXAMPLE=$$(echo $$EXAMPLE_PATHS | cut -d/ -f1)
        # This is a bit hacky and may not work in all bazel situations, but works for now
        if [[ $$EXAMPLE == "external" ]]; then
            EXAMPLE=$$(echo $$EXAMPLE_PATHS | cut -d/ -f-3)
        fi
        tar chf $@ -C . $$SHARED $(location %s) $$EXAMPLE
        """ % (shared, name, common_fun),
        tools = [
            common_fun,
            shared,
            "%s_files" % name,
        ],
    )

    native.sh_binary(
        name = "verify_%s" % name,
        srcs = [":verify_example.sh"],
        args = [
            name,
            "$(location :%s_dir)" % name,
        ],
        data = [":%s_dir" % name],
    )

def envoy_examples(examples):
    RESULTS = []
    RESULT_FILES = []

    native.filegroup(
        name = "shared_files",
        srcs = native.glob(
            ["shared/**/*"],
            exclude=[
                "**/*~",
                "**/.*",
                "**/#*",
                ".*/**/*",
            ],
        ),
    )

    for example in examples:
        envoy_example(name = example, shared = ":shared_files")
        native.genrule(
            name = "%s_result" % example,
            outs = ["%s_result.txt" % example],
            cmd = """
                ./$(location :verify_%s) %s $(location :%s_dir) >> $@
            """ % (example, example, example),
            tools = [
                "verify_%s" % example,
                "%s_dir" % example,
            ],
        )
        RESULTS.append("%s_result" % example)
        RESULT_FILES.append("$(location %s)" % ("%s_result" % example))

    native.sh_binary(
        name = "verify_examples",
        srcs = [":verify_examples.sh"],
        args = RESULT_FILES,
        data = RESULTS,
    )
