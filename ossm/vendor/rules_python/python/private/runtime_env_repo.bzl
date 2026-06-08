"""Internal setup to help the runtime_env toolchain."""

load("//python/private:repo_utils.bzl", "repo_utils")

def _runtime_env_repo_impl(rctx):
    pyenv = repo_utils.which_unchecked(rctx, "pyenv").binary
    if pyenv != None:
        pyenv_version_file = repo_utils.execute_checked(
            rctx,
            op = "GetPyenvVersionFile",
            arguments = [pyenv, "version-file"],
        ).stdout.strip()

        # When pyenv is used, the version file is what decided the
        # version used. Watch it so we compute the correct value if the
        # user changes it.
        rctx.watch(pyenv_version_file)

    version = repo_utils.execute_checked(
        rctx,
        op = "GetPythonVersion",
        arguments = [
            "python3",
            "-I",
            "-c",
            """import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")""",
        ],
        environment = {
            # Prevent the user's current shell from influencing the result.
            # This envvar won't be present when a test is run.
            # NOTE: This should be None, but Bazel 7 doesn't support None
            # values. Thankfully, pyenv treats empty string the same as missing.
            "PYENV_VERSION": "",
        },
    ).stdout.strip()
    rctx.file("info.bzl", "PYTHON_VERSION = '{}'\n".format(version))
    rctx.file("BUILD.bazel", "")

runtime_env_repo = repository_rule(
    implementation = _runtime_env_repo_impl,
)
