load("@rules_python//python:repositories.bzl", "python_register_toolchains")
load("//:versions.bzl", "VERSIONS")

def load_toolchains():
    python_register_toolchains(
        name = "python%s" % VERSIONS["python"].replace(".", "_"),
        python_version = VERSIONS["python"].replace("-", "_"),
    )
