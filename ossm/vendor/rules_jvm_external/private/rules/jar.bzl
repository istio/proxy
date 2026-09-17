load("@bazel_skylib//rules:run_binary.bzl", "run_binary")

def create_jar(name, inputs, out = None):
    if out == None:
        out = name + ".jar"
    elif not out.endswith(".jar"):
        out = out + ".jar"

    run_binary(
        name = name,
        srcs = inputs,
        outs = [out],
        tool = "@rules_jvm_external//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:CreateJar",
        args = ["$(location %s)" % out] + ["$(location %s)" % i for i in inputs],
    )
